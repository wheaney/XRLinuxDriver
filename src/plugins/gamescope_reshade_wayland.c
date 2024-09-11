
#include "epoch.h"
#include "files.h"
#include "imu.h"
#include "logging.h"
#include "plugins/gamescope_reshade_wayland.h"
#include "runtime_context.h"
#include "strings.h"
#include "wl_client/gamescope_reshade.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define GAMESCOPE_RESHADE_INTERFACE_NAME "gamescope_reshade"
#define GAMESCOPE_RESHADE_EFFECT_PATH "Sombrero.frag"
#define GAMESCOPE_RESHADE_WAIT_TIME_MS 500

static struct wl_display *display = NULL;
static struct wl_registry *registry = NULL;
static struct gamescope_reshade *reshade_object = NULL;
static gamescope_reshade_effect_ready_callback effect_ready_callback = NULL;
static uint64_t gamescope_reshade_effect_request_time = 0;
static bool gamescope_reshade_ipc_connected = false;

static void gamescope_reshade_wl_handle_global(void *data, struct wl_registry *registry,
                       uint32_t name, const char *interface, uint32_t version) {
    if (config()->debug_ipc) log_debug("gamescope_reshade_wl_handle_global %s\n", interface);
    if (strcmp(interface, GAMESCOPE_RESHADE_INTERFACE_NAME) == 0) {
        reshade_object = wl_registry_bind(registry, name, 
                                          &gamescope_reshade_interface, version);
    }
}

bool is_gamescope_reshade_ipc_connected() {
    return gamescope_reshade_ipc_connected;
}

static void gamescope_reshade_wl_server_disconnect() {
    gamescope_reshade_ipc_connected = false;
    if (config()->debug_ipc) log_debug("gamescope_reshade_wl_server_disconnect\n");
    if (effect_ready_callback) effect_ready_callback = NULL;
    if (reshade_object) {
        gamescope_reshade_destroy(reshade_object);
        reshade_object = NULL;
    }
    if (registry) {
        wl_registry_destroy(registry);
        registry = NULL;
    }
    if (display) {
        wl_display_flush(display);
        wl_display_disconnect(display);
        display = NULL;
    }
}

static void gamescope_reshade_wl_server_connect() {
    if (gamescope_reshade_ipc_connected) return;

    if (config()->debug_ipc) log_debug("gamescope_reshade_wl_server_connect\n");
    if (!display) display = wl_display_connect("gamescope-0");
    if (!display) {
        if (config()->debug_ipc) log_debug("gamescope_reshade_wl_server_connect no display\n");
        return;
    }
    
    if (!registry) registry = wl_display_get_registry(display);
    if (!registry) {
        if (config()->debug_ipc) log_debug("gamescope_reshade_wl_server_connect no registry\n");
        wl_display_disconnect(display);
        return;
    }
    
    if (!reshade_object) {
        static const struct wl_registry_listener registry_listener = {
            .global = gamescope_reshade_wl_handle_global
        };
    
        wl_registry_add_listener(registry, &registry_listener, NULL);
        wl_display_roundtrip(display);
    }

    if (reshade_object) {
        wl_display_dispatch_pending(display);
        wl_display_flush(display);
        int wl_display_error = wl_display_get_error(display);
        if (wl_display_error != 0) {
            if (config()->debug_ipc) log_debug("gamescope_reshade_wl_server_connect wl_display_error: %d\n", wl_display_error);
            gamescope_reshade_wl_server_disconnect();
            return;
        }

        gamescope_reshade_set_effect(reshade_object, GAMESCOPE_RESHADE_EFFECT_PATH);
        int wl_result = wl_display_flush(display);
        if (wl_result >= 0) {
            gamescope_reshade_effect_request_time = get_epoch_time_ms();
            gamescope_reshade_ipc_connected = true;
        } else {
            if (config()->debug_ipc) log_debug("gamescope_reshade_wl_server_connect error %d on wl_display_flush\n", wl_result);
            gamescope_reshade_wl_server_disconnect();
        }

    } else {
        if (config()->debug_ipc) log_debug("gamescope_reshade_wl_server_connect no reshade_object\n");
        gamescope_reshade_wl_server_disconnect();
    }
}

static bool gamescope_reshade_wl_setup_ipc() {
    if (gamescope_reshade_ipc_connected) gamescope_reshade_wl_server_connect();

    return true;
}

static bool enable_gamescope_effect() {
    if (!reshade_object) return false;

    if (config()->debug_ipc) log_debug("enable_gamescope_effect\n");
    gamescope_reshade_enable_effect(reshade_object);
    wl_display_flush(display);

    return true;
}

static void disable_gamescope_effect() {
    if (!reshade_object) return;
    
    if (config()->debug_ipc) log_debug("disable_gamescope_effect\n");
    gamescope_reshade_disable_effect(reshade_object);
    wl_display_flush(display);
}

static void wayland_cleanup() {
    disable_gamescope_effect();

    // set this early so other plugins can check if it's connected and change runtime variables
    // before the actual connection is closed
    gamescope_reshade_ipc_connected = false;
    plugins.handle_ipc_change();
    
    gamescope_reshade_wl_server_disconnect();
    gamescope_reshade_effect_request_time = 0;
}

void set_gamescope_reshade_effect_uniform_variable(const char *variable_name, const void *data, int entries, size_t element_size, bool flush) {
    if (!reshade_object) return;

    struct wl_array array;
    wl_array_init(&array);
    size_t total_size = entries * element_size;
    void *array_data = wl_array_add(&array, total_size);
    if (!array_data) {
        wl_array_release(&array);
        return;
    }
    memcpy(array.data, data, total_size);

    gamescope_reshade_set_uniform_variable(reshade_object, variable_name, &array);
    wl_array_release(&array);

    if (flush) {
        int wl_result;
        if (effect_ready_callback) {
            // this is a blocking call, so only use it if we're waiting on an event callback
            wl_result = wl_display_roundtrip(display);
        } else {
            wl_result = wl_display_flush(display);
        }

        if (wl_result < 0) {
            log_error("Error %d on gamescope wl_display_flush\n", wl_result);
            wayland_cleanup();
        }
    }
}

static void _effect_ready_callback(void *data,
                                   struct gamescope_reshade *gamescope_reshade,
                                   const char *effect_path) {
    if (config()->debug_ipc) log_debug("_effect_ready_callback: %s\n", effect_path);
    if (effect_ready_callback && equal(effect_path, GAMESCOPE_RESHADE_EFFECT_PATH)) {
        effect_ready_callback();
        effect_ready_callback = NULL;
    }
}

static void add_gamescope_effect_ready_listener(gamescope_reshade_effect_ready_callback callback) {
    if (!reshade_object) return;

    effect_ready_callback = callback;
    static const struct gamescope_reshade_listener listener = {
        .effect_ready = _effect_ready_callback
    };

    gamescope_reshade_add_listener(reshade_object, &listener, NULL);
}

static void _gamescope_reshade_effect_ready() {
    gamescope_reshade_effect_request_time = 0;
}

void gamescope_reshade_wl_handle_state_func() {
    if (device_present()) {
        if (!gamescope_reshade_ipc_connected) {
            gamescope_reshade_wl_server_connect();
            if (gamescope_reshade_ipc_connected) {
                if (config()->debug_ipc) log_debug("gamescope_reshade_wl_handle_state_func connected to gamescope\n");
                plugins.handle_ipc_change();

                add_gamescope_effect_ready_listener(_gamescope_reshade_effect_ready);
                enable_gamescope_effect();
            }
        }
    } else {
        if (gamescope_reshade_ipc_connected) wayland_cleanup();
    }
};

void gamescope_reshade_wl_handle_imu_data_func(uint32_t timestamp_ms, imu_quat_type quat, 
                                               imu_euler_type velocities, bool imu_calibrated, 
                                               ipc_values_type *ipc_values) {
    if (!reshade_object) return;
    
    if (gamescope_reshade_effect_request_time != 0 && 
            get_epoch_time_ms() - gamescope_reshade_effect_request_time > 
            GAMESCOPE_RESHADE_WAIT_TIME_MS) {
        log_error("gamescope effect_ready event never received, falling back to shared memory IPC\n");
        wayland_cleanup();
    }
}

void gamescope_reshade_wl_reset_imu_data_func() {
    set_gamescope_reshade_effect_uniform_variable("imu_quat_data", imu_reset_data, 16, sizeof(float), true);
}

const plugin_type gamescope_reshade_wayland_plugin = {
    .id = "gamescope_reshade_wayland",
    .setup_ipc = gamescope_reshade_wl_setup_ipc,
    .handle_state = gamescope_reshade_wl_handle_state_func,
    .handle_imu_data = gamescope_reshade_wl_handle_imu_data_func,
    .reset_imu_data = gamescope_reshade_wl_reset_imu_data_func,
    .handle_device_disconnect = wayland_cleanup,
};