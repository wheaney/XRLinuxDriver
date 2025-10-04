#include "config.h"
#include "logging.h"
#include "plugins.h"
#include "plugins/opentrack_source.h"
#include "runtime_context.h"
#include "strings.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>

static opentrack_source_config *ot_config = NULL;
static int udp_fd = -1;
static struct sockaddr_storage udp_addr;
static socklen_t udp_addr_len = 0;
static uint32_t frame_number = 0;

static void opentrack_close_socket() {
    if (udp_fd != -1) {
        close(udp_fd);
        udp_fd = -1;
    }
    udp_addr_len = 0;
}

static bool opentrack_open_socket(const char *ip, int port) {
    opentrack_close_socket();

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;   // UDP
    hints.ai_flags = 0;               // No special flags; numeric or hostname

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    // Accept IPv6 literals optionally wrapped in brackets like [::1]
    const char *host = ip;
    char hostbuf[256];
    if (!host || host[0] == '\0') {
        host = "127.0.0.1";
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
        log_error("opentrack: getaddrinfo('%s', %d) failed: %s\n", host, port, gai_strerror(rc));
        return false;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        udp_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (udp_fd < 0)
            continue;

        memset(&udp_addr, 0, sizeof(udp_addr));
        memcpy(&udp_addr, rp->ai_addr, rp->ai_addrlen);
        udp_addr_len = (socklen_t)rp->ai_addrlen;
        break;
    }

    if (rp == NULL) {
        log_error("opentrack: socket() failed for all addresses for '%s': %s\n", ip, strerror(errno));
        freeaddrinfo(res);
        return false;
    }

    freeaddrinfo(res);
    return true;
}

static void *opentrack_default_config_func() {
    opentrack_source_config *cfg = calloc(1, sizeof(opentrack_source_config));
    cfg->enabled = false; // opt-in
    cfg->ip = strdup("127.0.0.1");
    cfg->port = 4242; // OpenTrack default for UDP over network
    return cfg;
}

static void opentrack_handle_config_line_func(void *config, char *key, char *value) {
    opentrack_source_config *cfg = (opentrack_source_config *)config;
    if (!cfg) return;

    // Preferred keys
    if (equal(key, "opentrack_app_ip")) {
        string_config(key, value, &cfg->ip);
    } else if (equal(key, "opentrack_app_port")) {
        int_config(key, value, &cfg->port);
    }
}

static void opentrack_set_config_func(void *new_config) {
    opentrack_source_config *new_cfg = (opentrack_source_config *)new_config;
    if (!new_cfg) return;

    bool first = ot_config == NULL;
    bool reopen = false;

    new_cfg->enabled = in_array("opentrack", config()->external_modes, config()->external_modes_count);
    if (!first) {
        if (ot_config->enabled != new_cfg->enabled)
            log_message("OpenTrack UDP has been %s\n", new_cfg->enabled ? "enabled" : "disabled");
        if (strcmp(ot_config->ip ? ot_config->ip : "", new_cfg->ip ? new_cfg->ip : "") != 0) {
            log_message("OpenTrack IP changed to %s\n", new_cfg->ip);
            reopen = true;
        }
        if (ot_config->port != new_cfg->port) {
            log_message("OpenTrack port changed to %d\n", new_cfg->port);
            reopen = true;
        }

        free(ot_config->ip);
        free(ot_config);
    }

    ot_config = new_cfg;

    if (ot_config->enabled) {
        if (first || reopen || udp_fd == -1) {
            if (opentrack_open_socket(ot_config->ip, ot_config->port)) {
                log_message("OpenTrack UDP target: %s:%d\n", ot_config->ip, ot_config->port);
            }
        }
    } else {
        opentrack_close_socket();
    }
}

static void opentrack_handle_device_disconnect_func() {
    // keep socket open; opentrack may still accept data for other sources
    frame_number = 0;
}

static void opentrack_handle_imu_data_func(uint32_t timestamp_ms, imu_quat_type quat, imu_euler_type euler,
                                           imu_euler_type velocities, bool imu_calibrated, ipc_values_type *ipc_values) {
    (void)timestamp_ms;
    (void)ipc_values;

    if (!ot_config || !ot_config->enabled || !imu_calibrated || udp_fd == -1) return;

    // Map to OpenTrack expected payload: 6 doubles (x,y,z,yaw,pitch,roll) + uint32 frame number
    // Units: the PHP PoC scaled position by 10 and used degrees for yaw/pitch/roll.
    // We don't have positional tracking, so set x,y,z to 0.
    double payload[6];
    payload[0] = 0.0; // x
    payload[1] = 0.0; // y
    payload[2] = 0.0; // z
    payload[3] = euler.yaw;
    payload[4] = euler.pitch;
    payload[5] = euler.roll;

    // Build buffer: 6 doubles + uint32
    uint8_t buffer[6 * sizeof(double) + sizeof(uint32_t)];
    memcpy(buffer, payload, 6 * sizeof(double));
    uint32_t fn = frame_number++;
    memcpy(buffer + 6 * sizeof(double), &fn, sizeof(uint32_t));

    ssize_t to_send = sizeof(buffer);
    ssize_t sent = sendto(udp_fd, buffer, to_send, 0, (struct sockaddr *)&udp_addr, udp_addr_len);
    if (sent < 0) {
        static int err_count = 0;
        if (err_count++ % 100 == 0) // rate limit errors
            log_error("opentrack: sendto failed: %s\n", strerror(errno));
    }
}

static void opentrack_reset_imu_data_func() {
    frame_number = 0;
}

const plugin_type opentrack_source_plugin = {
    .id = "opentrack_source",
    .default_config = opentrack_default_config_func,
    .handle_config_line = opentrack_handle_config_line_func,
    .set_config = opentrack_set_config_func,
    .handle_imu_data = opentrack_handle_imu_data_func,
    .reset_imu_data = opentrack_reset_imu_data_func,
    .handle_device_disconnect = opentrack_handle_device_disconnect_func
};
