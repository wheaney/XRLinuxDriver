#include "config.h"
#include "connection_pool.h"
#include "devices.h"
#include "driver.h"
#include "imu.h"
#include "logging.h"
#include "plugins/opentrack_listener.h"
#include "runtime_context.h"
#include "strings.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

#define OT_DEVICE_BRAND "OpenTrack"
#define OT_DRIVER_ID "opentrack"

// This plugin exposes an OpenTrack UDP stream as a synthetic IMU device.
// It binds a UDP socket, blocks waiting for 6-double payloads (x,y,z,yaw,pitch,roll),
// converts yaw/pitch/roll (degrees) to a quaternion, and feeds the driver loop.

static opentrack_listener_config *ot_cfg = NULL;

// Socket state
static int udp_fd = -1;
static struct sockaddr_storage bind_addr;
static socklen_t bind_addr_len = 0;
static volatile bool connected = false;
static pthread_t listener_thread;
static bool listener_running = false;
static pthread_mutex_t conn_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t conn_cond = PTHREAD_COND_INITIALIZER;
static volatile bool block_active = false;

// Track OpenTrack source plugin config to prevent feedback loops
static bool ot_source_enabled = false;
static char *ot_source_ip = NULL;
static volatile bool feedback_loop_ignore = false;

static void listener_device_disconnect() {
    connection_t* c = connection_pool_find_driver_connection(OT_DRIVER_ID);
    if (c) {
        connected_device_type* connected_device = calloc(1, sizeof(connected_device_type));
        connected_device->driver = c->driver;
        connected_device->device = c->device;
        handle_device_connection_changed(false, connected_device);
    }

    pthread_mutex_lock(&conn_mutex);
    if (connected) {
        connected = false;
        pthread_cond_broadcast(&conn_cond);
    }
    pthread_mutex_unlock(&conn_mutex);
}

// Utility: check if a target IP string refers to localhost/unspecified
static bool is_localhostish_ip(const char *ip_raw) {
    // defaults to localhost if unset, return true
    if (!ip_raw || !ip_raw[0]) return true;

    // make a scratch copy to strip brackets and zone-id
    char buf[256];
    size_t len = strlen(ip_raw);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, ip_raw, len);
    buf[len] = '\0';

    // Strip surrounding [ ] for IPv6 literals
    if (buf[0] == '[') {
        size_t blen = strlen(buf);
        if (blen >= 2 && buf[blen - 1] == ']') {
            memmove(buf, buf + 1, blen - 2);
            buf[blen - 2] = '\0';
        }
    }
    // Strip zone id (e.g., fe80::1%lo0)
    char *pct = strchr(buf, '%');
    if (pct) *pct = '\0';

    // Quick textual checks
    if (strcmp(buf, "localhost") == 0) return true;
    if (strcmp(buf, "0.0.0.0") == 0) return true; // unspecified IPv4
    if (strcmp(buf, "::") == 0) return true;      // unspecified IPv6

    // Parse as IPv4
    struct in_addr a4;
    if (inet_pton(AF_INET, buf, &a4) == 1) {
        uint32_t host = ntohl(a4.s_addr);
        if (host == 0) return true;               // 0.0.0.0
        if ((host & 0xFF000000u) == 0x7F000000u)  // 127.0.0.0/8
            return true;
        return false;
    }

    // Parse as IPv6
    struct in6_addr a6;
    if (inet_pton(AF_INET6, buf, &a6) == 1) {
        if (IN6_IS_ADDR_LOOPBACK(&a6)) return true;     // ::1
        if (IN6_IS_ADDR_UNSPECIFIED(&a6)) return true;  // ::
        return false;
    }

    // Unknown format; be conservative and do not consider it localhost
    return false;
}

static void update_feedback_guard() {
    bool prev = feedback_loop_ignore;
    feedback_loop_ignore = (ot_cfg && ot_cfg->enabled && ot_source_enabled && is_localhostish_ip(ot_source_ip));
    if (feedback_loop_ignore && !prev) {
        log_message("OpenTrack listener: ignoring loopback packets (source enabled; target %s)\n", ot_source_ip ? ot_source_ip : "127.0.0.1");
        listener_device_disconnect();
    } else if (!feedback_loop_ignore && prev) {
        log_message("OpenTrack listener: feedback guard disabled; accepting packets\n");
    }
}

// Placeholder device properties for an OpenTrack source
static device_properties_type *make_opentrack_device_properties() {
    device_properties_type *d = calloc(1, sizeof(*d));
    d->brand = OT_DEVICE_BRAND;
    d->model = "UDP";
    d->hid_vendor_id = 0;
    d->hid_product_id = 0;
    d->usb_bus = 0;
    d->usb_address = 0;
    d->calibration_setup = CALIBRATION_SETUP_AUTOMATIC;
    d->resolution_w = RESOLUTION_1080P_W;
    d->resolution_h = RESOLUTION_1080P_H;
    d->fov = 45.0f;
    d->lens_distance_ratio = 0.03125f;
    d->calibration_wait_s = 1;
    d->imu_cycles_per_s = 120; // nominal rate; actual rate comes from incoming stream timing
    d->imu_buffer_size = 1;
    d->look_ahead_constant = 25.0f;
    d->look_ahead_frametime_multiplier = 0.3f;
    d->look_ahead_scanline_adjust = 8.0f;
    d->look_ahead_ms_cap = 40.0f;
    d->sbs_mode_supported = false;
    d->firmware_update_recommended = false;
    d->can_be_supplemental = true;
    d->provides_orientation = true;
    d->provides_position = true;
    return d;
}

static void opentrack_close_socket() {
    if (udp_fd != -1) {
        close(udp_fd);
        udp_fd = -1;
    }
    bind_addr_len = 0;
}

static bool opentrack_bind_socket(const char *ip, int port) {
    opentrack_close_socket();

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    const char *host = ip;
    char hostbuf[256];
    if (!host || host[0] == '\0') {
        host = NULL;
    } else {
        size_t len = strlen(host);
        if (len >= 2 && host[0] == '[' && host[len - 1] == ']') {
            size_t n = len - 2;
            if (n >= sizeof(hostbuf)) n = sizeof(hostbuf) - 1;
            memcpy(hostbuf, host + 1, n);
            hostbuf[n] = '\0';
            host = hostbuf;
        }
    }

    struct addrinfo *res = NULL, *rp = NULL;
    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0) {
        log_error("opentrack (consumer): getaddrinfo('%s', %d) failed: %s\n", host ? host : "*", port, gai_strerror(rc));
        return false;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        udp_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (udp_fd < 0) continue;

        int one = 1;
        setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        if (rp->ai_family == AF_INET6) {
            int v6only = 0;
            (void)setsockopt(udp_fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
        }

        if (bind(udp_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            memset(&bind_addr, 0, sizeof(bind_addr));
            memcpy(&bind_addr, rp->ai_addr, rp->ai_addrlen);
            bind_addr_len = (socklen_t)rp->ai_addrlen;
            break;
        } else {
            if (config()->debug_device)
                log_debug("opentrack listener: bind failed: %s\n", strerror(errno));
            close(udp_fd);
            udp_fd = -1;
        }
    }

    if (rp == NULL) {
    log_error("opentrack listener: failed to bind %s:%d\n", ip && ip[0] ? ip : "*", port);
        freeaddrinfo(res);
        return false;
    }

    freeaddrinfo(res);
    return true;
}

// --- device driver impl ---
static device_properties_type *opentrack_supported_device(uint16_t id_vendor, uint16_t id_product, uint8_t usb_bus, uint8_t usb_address) {
    (void)id_vendor; (void)id_product; (void)usb_bus; (void)usb_address;
    // This hook is specific to the libusb hotplug event, which will never be triggered for the OpenTrack source.
    return NULL;
}

static bool opentrack_device_connect() {
    connected = ot_cfg && ot_cfg->enabled && udp_fd != -1;
    return connected;
}

static void opentrack_block_on_device() {
    device_properties_type *device = device_checkout();
    pthread_mutex_lock(&conn_mutex);
    block_active = true;
    while (connected) {
        pthread_cond_wait(&conn_cond, &conn_mutex);
    }
    block_active = false;
    pthread_mutex_unlock(&conn_mutex);
    device_checkin(device);
}

static bool opentrack_device_is_sbs_mode() { return false; }
static bool opentrack_device_set_sbs_mode(bool enabled) { (void)enabled; return false; }
static bool opentrack_is_connected() { return connected; }
static void opentrack_disconnect(bool soft) {
    pthread_mutex_lock(&conn_mutex);
    connected = false;
    pthread_cond_broadcast(&conn_cond);
    pthread_mutex_unlock(&conn_mutex);
}

static const device_driver_type opentrack_driver = {
    .id                                 = OT_DRIVER_ID,
    .supported_device_func              = opentrack_supported_device,
    .device_connect_func                = opentrack_device_connect,
    .block_on_device_func               = opentrack_block_on_device,
    .device_is_sbs_mode_func            = opentrack_device_is_sbs_mode,
    .device_set_sbs_mode_func           = opentrack_device_set_sbs_mode,
    .is_connected_func                  = opentrack_is_connected,
    .disconnect_func                    = opentrack_disconnect
};

// --- plugin impl ---
static void *opentrack_default_config_func() {
    opentrack_listener_config *cfg = calloc(1, sizeof(*cfg));
    cfg->enabled = false;
    cfg->ip = strdup("0.0.0.0");
    cfg->port = 4242;
    return cfg;
}

static void opentrack_handle_config_line_func(void *config, char *key, char *value) {
    opentrack_listener_config *cfg = (opentrack_listener_config *)config;
    if (!cfg) return;

    if (equal(key, "opentrack_listener_enabled")) {
        boolean_config(key, value, &cfg->enabled);
    } else if (equal(key, "opentrack_listen_ip")) {
        string_config(key, value, &cfg->ip);
    } else if (equal(key, "opentrack_listen_port")) {
        int_config(key, value, &cfg->port);
    } else if (equal(key, "external_mode")) {
        ot_source_enabled = list_string_contains("opentrack", value);
    } else if (equal(key, "opentrack_app_ip")) {
        string_config(key, value, &ot_source_ip);
    }
}

#define OT_UDP_TIMEOUT_MS 500
static const struct timeval OT_SELECT_TIMEOUT = {
    .tv_sec = 0,
    .tv_usec = OT_UDP_TIMEOUT_MS * 1000
};
static void* opentrack_listener_thread_func(void* arg) {
    (void)arg;
    uint8_t buf[6 * sizeof(double) + sizeof(uint32_t)];
    while (!driver_disabled() && ot_cfg && ot_cfg->enabled && udp_fd != -1) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(udp_fd, &rfds);
        struct timeval tv = OT_SELECT_TIMEOUT;
        int sel = select(udp_fd + 1, &rfds, NULL, NULL, &tv);
        if (sel == 0) {
            // timed out
            if (connected) listener_device_disconnect();
            continue;
        } else if (sel < 0) {
            if (errno == EINTR) continue;
            if (config()->debug_device)
                log_debug("opentrack listener: select error: %s\n", strerror(errno));
            usleep(1000);
            continue;
        }

        ssize_t n = recvfrom(udp_fd, buf, sizeof(buf), 0, NULL, NULL);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (config()->debug_device)
                log_debug("opentrack listener: recvfrom error: %s\n", strerror(errno));
            usleep(1000);
            continue;
        }
        if (n < (ssize_t)(6 * sizeof(double))) continue;

        // If we're guarding against a feedback loop, discard packets silently
        if (feedback_loop_ignore) {
            continue;
        }

        if (!connected && !device_present()) {
            connected_device_type *nd = calloc(1, sizeof(connected_device_type));
            nd->driver = &opentrack_driver;
            nd->device = make_opentrack_device_properties();
            handle_device_connection_changed(true, nd);
            pthread_mutex_lock(&conn_mutex);
            connected = true;
            pthread_mutex_unlock(&conn_mutex);
        }

        static bool prev_block_active = false;
        if (block_active) {
            device_properties_type *device = device_checkout();
            if (device) {
                // Only feed events when block_on_device is active
                double vals[6];
                memcpy(vals, buf, 6 * sizeof(double));
                imu_euler_type e = { .roll = (float)vals[5], .pitch = (float)vals[4], .yaw = (float)vals[3] };
                imu_quat_type q = euler_to_quaternion_zyx(e);
                struct timeval tv2; gettimeofday(&tv2, NULL);
                uint32_t ts_ms = (uint32_t)((tv2.tv_sec * 1000ULL) + (tv2.tv_usec / 1000ULL));
                static uint32_t start_ts_ms = 0; // set on first active event
                if (!prev_block_active) {
                    start_ts_ms = ts_ms;
                }

                // OpenTrack reports in EUS; convert to NWU
                float full_distance_cm = LENS_TO_PIVOT_CM / device->lens_distance_ratio;
                imu_vec3_type pos = { 
                    .x = (float)-vals[2] / full_distance_cm,
                    .y = (float)-vals[0] / full_distance_cm,
                    .z = (float)vals[1] / full_distance_cm
                };

                imu_pose_type pose = (imu_pose_type){0};
                pose.orientation = q;
                pose.position = pos;
                pose.has_orientation = true;
                pose.has_position = true;
                pose.timestamp_ms = ts_ms - start_ts_ms;
                driver_handle_pose_event(OT_DRIVER_ID, pose);
            }
            device_checkin(device);
        }
        prev_block_active = block_active;
    }

    listener_running = false;
    return NULL;
}

static void opentrack_start_func() {
    // If disabled, stop listener and close socket
    if (driver_disabled() || !ot_cfg || !ot_cfg->enabled) {
        listener_device_disconnect();
        opentrack_close_socket();
        if (listener_running) {
            pthread_join(listener_thread, NULL);
            listener_running = false;
        }
        return;
    }

    // Enabled: ensure bound
    if (udp_fd == -1) {
        const char *ip = ot_cfg->ip && ot_cfg->ip[0] ? ot_cfg->ip : NULL;
        int port = ot_cfg->port > 0 ? ot_cfg->port : 4242;
        if (!opentrack_bind_socket(ip, port)) {
            log_error("OpenTrack listener: failed to bind on %s:%d; leaving disabled\n", ip ? ip : "*", port);
            return;
        }
        char ipstr[128] = {0};
        if (ip && ip[0]) strncpy(ipstr, ip, sizeof(ipstr)-1); else strncpy(ipstr, "*", sizeof(ipstr)-1);
        log_message("OpenTrack listener bound on %s:%d\n", ipstr, port);
    }

    // Start listener thread to manage registration, feeding, and idle disconnect
    if (!listener_running) {
        listener_running = true;
        pthread_create(&listener_thread, NULL, opentrack_listener_thread_func, NULL);
    }
}

static void opentrack_set_config_func(void *new_config) {
    opentrack_listener_config *new_cfg = (opentrack_listener_config *)new_config;
    if (!new_cfg) return;

    bool first = (ot_cfg == NULL);
    bool was_enabled = ot_cfg && ot_cfg->enabled;

    if (!first) {
        if (was_enabled != new_cfg->enabled)
            log_message("OpenTrack listener has been %s\n", new_cfg->enabled ? "enabled" : "disabled");
        free(ot_cfg->ip);
        free(ot_cfg);
    }
    ot_cfg = new_cfg;

    opentrack_start_func();
    update_feedback_guard();
}

static void opentrack_handle_device_disconnect_func() {
    opentrack_disconnect(false);
}

const plugin_type opentrack_listener_plugin = {
    .id = "opentrack_listener",
    .default_config = opentrack_default_config_func,
    .handle_config_line = opentrack_handle_config_line_func,
    .set_config = opentrack_set_config_func,
    .start = opentrack_start_func,
    .handle_device_disconnect = opentrack_handle_device_disconnect_func
};
