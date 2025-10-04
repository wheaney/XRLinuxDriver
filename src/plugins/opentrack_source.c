#include "config.h"
#include "logging.h"
#include "plugins.h"
#include "plugins/opentrack_source.h"

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
#include <unistd.h>

static opentrack_source_config *ot_config = NULL;
static int udp_fd = -1;
static struct sockaddr_in udp_addr;
static uint32_t frame_number = 0;

static void opentrack_close_socket() {
    if (udp_fd != -1) {
        close(udp_fd);
        udp_fd = -1;
    }
}

static bool opentrack_open_socket(const char *ip, int port) {
    opentrack_close_socket();

    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) {
        log_error("opentrack: socket() failed: %s\n", strerror(errno));
        return false;
    }

    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &udp_addr.sin_addr) != 1) {
        log_error("opentrack: invalid ip '%s'\n", ip);
        opentrack_close_socket();
        return false;
    }

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
    if (equal(key, "opentrack_source_enabled")) {
        boolean_config(key, value, &cfg->enabled);
    } else if (equal(key, "opentrack_app_ip")) {
        string_config(key, value, &cfg->ip);
    } else if (equal(key, "opentrack_app_port")) {
        int_config(key, value, &cfg->port);
    }
}

static void opentrack_set_config_func(void *config) {
    opentrack_source_config *new_cfg = (opentrack_source_config *)config;
    if (!new_cfg) return;

    bool first = ot_config == NULL;
    bool reopen = false;

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
    ssize_t sent = sendto(udp_fd, buffer, to_send, 0, (struct sockaddr *)&udp_addr, sizeof(udp_addr));
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
