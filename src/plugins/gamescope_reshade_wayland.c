#include "epoch.h"
#include "files.h"
#include "imu.h"
#include "logging.h"
#include "plugins/gamescope_reshade_wayland.h"
#include "runtime_context.h"
#include "strings.h"
#include "wl_client/gamescope_reshade.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

/**
 * The wayland-client library may not be present, so we create a weak reference to wl_proxy_create just
 * for the sake of checking if the library is linked. All function calls should be preceeded by a check
 * for the presence of wl_proxy_create or the presence of the reshade_object (which implies the presence 
 * of the former has already succeeded previously).
 */
#include <wayland-client.h>
__attribute__((weak)) struct wl_proxy *wl_proxy_create(struct wl_proxy *factory, const struct wl_interface *interface);

#define GAMESCOPE_RESHADE_INTERFACE_NAME "gamescope_reshade"
#define GAMESCOPE_RESHADE_EFFECT_FILE "Sombrero.frag"
#define GAMESCOPE_RESHADE_EFFECT_PATH "reshade/Shaders/" GAMESCOPE_RESHADE_EFFECT_FILE
#define GAMESCOPE_RESHADE_WAIT_TIME_MS 500

static gamescope_reshade_wayland_config *gamescope_config;
static struct wl_display *display = NULL;
static struct wl_registry *registry = NULL;
static struct gamescope_reshade *reshade_object = NULL;
static gamescope_reshade_effect_ready_callback effect_ready_callback = NULL;
static uint64_t gamescope_reshade_effect_request_time = 0;
static bool gamescope_reshade_ipc_connected = false;
static pthread_mutex_t wayland_mutex = PTHREAD_MUTEX_INITIALIZER;

void *gamescope_reshade_wayland_default_config_func() {
    gamescope_reshade_wayland_config *config = calloc(1, sizeof(gamescope_reshade_wayland_config));
    config->disabled = false;

    return config;
};

void gamescope_reshade_wayland_handle_config_line_func(void* config, char* key, char* value) {
    gamescope_reshade_wayland_config* temp_config = (gamescope_reshade_wayland_config*) config;

    if (equal(key, "gamescope_reshade_wayland_disabled")) {
        boolean_config(key, value, &temp_config->disabled);
    }
};

void gamescope_reshade_wayland_set_config_func(void* config) {
    if (!config) return;
    gamescope_reshade_wayland_config* temp_config = (gamescope_reshade_wayland_config*) config;

    if (gamescope_config) {
        if (gamescope_config->disabled != temp_config->disabled)
            log_message("Gamescope ReShade integration has been %s\n", temp_config->disabled ? "disabled" : "enabled");

        free(gamescope_config);
    }
    gamescope_config = temp_config;
};

static char* sombrero_shader_file_path() {
    static char* shader_file_path = NULL;
    if (!shader_file_path)
        shader_file_path = get_xdg_file_path_for_app("gamescope", GAMESCOPE_RESHADE_EFFECT_PATH, XDG_DATA_ENV_VAR, XDG_DATA_FALLBACK_DIR);

    return shader_file_path;
}

static bool sombrero_file_exists() {
    return access(sombrero_shader_file_path(), F_OK) != -1;
}

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

static void do_wl_server_disconnect() {
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

static bool do_wl_server_connect() {
    if (gamescope_config->disabled || gamescope_reshade_ipc_connected) return false;

    if (config()->debug_ipc) log_debug("gamescope_reshade_wl_server_connect\n");
    if (wl_proxy_create == NULL) {
        if (config()->debug_ipc)
            log_debug("gamescope_reshade_wl_server_connect wayland-client library not present\n");
        return false;
    }

    if (!display) display = wl_display_connect("gamescope-0");
    if (!display) {
        if (config()->debug_ipc) log_debug("gamescope_reshade_wl_server_connect no display\n");
        return false;
    }
    
    if (!registry) registry = wl_display_get_registry(display);
    if (!registry) {
        if (config()->debug_ipc) log_debug("gamescope_reshade_wl_server_connect no registry\n");
        wl_display_disconnect(display);
        display = NULL;
        return false;
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
            if (config()->debug_ipc) 
                log_debug("gamescope_reshade_wl_server_connect wl_display_error: %d\n", wl_display_error);
            do_wl_server_disconnect();
            return false;
        }

        gamescope_reshade_set_effect(reshade_object, GAMESCOPE_RESHADE_EFFECT_FILE);
        int wl_result = wl_display_flush(display);
        if (wl_result < 0) {
            log_error("gamescope_reshade_wl_server_connect error %d on wl_display_flush: %s\n", wl_result, strerror(errno));
            do_wl_server_disconnect();
            return false;
        }

        gamescope_reshade_effect_request_time = get_epoch_time_ms();
        gamescope_reshade_ipc_connected = true;
        return true;
    } else {
        if (config()->debug_ipc) log_debug("gamescope_reshade_wl_server_connect no reshade_object\n");
        do_wl_server_disconnect();
        return false;
    }
}

static bool gamescope_reshade_wl_setup_ipc() {
    if (config()->debug_ipc) log_debug("gamescope_reshade_wl_setup_ipc\n");

    pthread_mutex_lock(&wayland_mutex);
    do_wl_server_connect();
    pthread_mutex_unlock(&wayland_mutex);

    return true;
}

static bool do_wl_enable_gamescope_effect() {
    if (!reshade_object) return false;

    if (config()->debug_ipc) log_debug("enable_gamescope_effect\n");
    gamescope_reshade_enable_effect(reshade_object);
    wl_display_flush(display);
    return true;
}

static bool do_wl_disable_gamescope_effect() {
    if (!reshade_object) return false;
    
    if (config()->debug_ipc) log_debug("disable_gamescope_effect\n");
    gamescope_reshade_disable_effect(reshade_object);
    wl_display_flush(display);
    
    return true;
}

static bool plugins_ipc_change_call_in_progress = false;

// must only be called from within the wayland mutex
static void do_trigger_plugins_ipc_change() {
    // prevent recursive calls to this function
    if (plugins_ipc_change_call_in_progress) return;

    plugins_ipc_change_call_in_progress = true;

    // we're sending control to outside plugins which may trigger other calls to set unifrom variables,
    // so we have to unlock the mutex to be safe and prevent deadlocks
    pthread_mutex_unlock(&wayland_mutex);
    plugins.handle_ipc_change();
    pthread_mutex_lock(&wayland_mutex);

    plugins_ipc_change_call_in_progress = false;
}

static void do_wl_cleanup() {
    do_wl_disable_gamescope_effect();

    // set this early so other plugins can check if it's connected and change runtime variables
    // before the actual connection is closed
    gamescope_reshade_ipc_connected = false;
    state()->is_gamescope_reshade_ipc_connected = false;
    do_trigger_plugins_ipc_change();
    
    do_wl_server_disconnect();
    gamescope_reshade_effect_request_time = 0;
}

static void wayland_cleanup() {
    if (config()->debug_ipc) log_debug("wayland_cleanup\n");

    pthread_mutex_lock(&wayland_mutex);
    do_wl_cleanup();
    pthread_mutex_unlock(&wayland_mutex);
}

static bool do_wl_set_uniform_variable(const char *variable_name, const void *data, int entries, 
                                       size_t element_size, bool flush) {
    if (!reshade_object) return false;

    struct wl_array array;
    wl_array_init(&array);
    size_t total_size = entries * element_size;
    void *array_data = wl_array_add(&array, total_size);
    if (!array_data) {
        wl_array_release(&array);
        return false;
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
            log_error("Error %d on gamescope wl_display_flush: %s\n", wl_result, strerror(errno));
            return false;
        }
    }
    
    return true;
}

static void _effect_ready_callback(void *data,
                                   struct gamescope_reshade *gamescope_reshade,
                                   const char *effect_path) {
    if (config()->debug_ipc) log_debug("_effect_ready_callback: %s\n", effect_path);
    if (effect_ready_callback && equal(effect_path, GAMESCOPE_RESHADE_EFFECT_FILE)) {
        effect_ready_callback();
        effect_ready_callback = NULL;
    }
}

static bool do_wl_add_gamescope_effect_ready_listener(gamescope_reshade_effect_ready_callback callback) {
    if (!reshade_object) return false;

    effect_ready_callback = callback;
    static const struct gamescope_reshade_listener listener = {
        .effect_ready = _effect_ready_callback
    };

    gamescope_reshade_add_listener(reshade_object, &listener, NULL);
    
    return true;
}

static void _gamescope_reshade_effect_ready() {
    gamescope_reshade_effect_request_time = 0;
}

void set_gamescope_reshade_effect_uniform_variable(const char *variable_name, const void *data, int entries, 
                                                   size_t element_size, bool flush) {
    if (!reshade_object) return;

    pthread_mutex_lock(&wayland_mutex);
    bool success = do_wl_set_uniform_variable(variable_name, data, entries, element_size, flush);
    if (!success && flush) {
        do_wl_cleanup();
    }
    pthread_mutex_unlock(&wayland_mutex);
}

void set_skippable_gamescope_reshade_effect_uniform_variable(const char *variable_name, const void *data, 
                                                             int entries, size_t element_size, bool flush) {
    // if already locked, just skip this call
    if (!reshade_object || pthread_mutex_trylock(&wayland_mutex) != 0) return;

    bool success = do_wl_set_uniform_variable(variable_name, data, entries, element_size, flush);
    if (!success && flush) {
        do_wl_cleanup();
    }
    pthread_mutex_unlock(&wayland_mutex);
}

void gamescope_reshade_wl_handle_state_func() {
    pthread_mutex_lock(&wayland_mutex);
    if (device_present() && sombrero_file_exists() && !gamescope_config->disabled) {
        if (!gamescope_reshade_ipc_connected) {
            do_wl_server_connect();
            if (gamescope_reshade_ipc_connected) {
                if (config()->debug_ipc) log_debug("gamescope_reshade_wl_handle_state_func connected to gamescope\n");
                state()->is_gamescope_reshade_ipc_connected = true;

                do_trigger_plugins_ipc_change();

                do_wl_add_gamescope_effect_ready_listener(_gamescope_reshade_effect_ready);
                do_wl_enable_gamescope_effect();
            }
        }
    } else {
        if (gamescope_reshade_ipc_connected) do_wl_cleanup();
    }
    pthread_mutex_unlock(&wayland_mutex);
};

void gamescope_reshade_wl_handle_imu_data_func(uint32_t timestamp_ms, imu_quat_type quat, 
                                               imu_euler_type euler, imu_euler_type velocities, bool imu_calibrated, 
                                               ipc_values_type *ipc_values) {
    if (!reshade_object) return;
    
    pthread_mutex_lock(&wayland_mutex);
    if (gamescope_reshade_effect_request_time != 0 && 
            get_epoch_time_ms() - gamescope_reshade_effect_request_time > 
            GAMESCOPE_RESHADE_WAIT_TIME_MS) {
        log_error("gamescope effect_ready event never received, falling back to shared memory IPC\n");
        do_wl_cleanup();
    }
    pthread_mutex_unlock(&wayland_mutex);
}

void gamescope_reshade_wl_reset_imu_data_func() {
    set_gamescope_reshade_effect_uniform_variable("imu_quat_data", imu_reset_data, 16, sizeof(float), true);
}

const plugin_type gamescope_reshade_wayland_plugin = {
    .id = "gamescope_reshade_wayland",
    .default_config = gamescope_reshade_wayland_default_config_func,
    .handle_config_line = gamescope_reshade_wayland_handle_config_line_func,
    .set_config = gamescope_reshade_wayland_set_config_func,
    .setup_ipc = gamescope_reshade_wl_setup_ipc,
    .handle_state = gamescope_reshade_wl_handle_state_func,
    .handle_imu_data = gamescope_reshade_wl_handle_imu_data_func,
    .reset_imu_data = gamescope_reshade_wl_reset_imu_data_func,
    .handle_device_disconnect = wayland_cleanup,
};