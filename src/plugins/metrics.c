#include "config.h"
#include "curl.h"
#include "devices.h"
#include "plugins.h"
#include "runtime_context.h"
#include "system.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *UA_MEASUREMENT_ID = "G-Z94MXP18T6";
const char *UA_CLIENT_ID="ARLinuxDriver";
void log_metric(char *event_name) {
    #ifdef UA_API_SECRET
        CURL *curl;
        CURLcode res;

        struct curl_slist *headers = NULL;

        curl_init();

        curl = curl_easy_init();
        if(curl) {
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
            headers = curl_slist_append(headers, "Content-Type: application/json");

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            char url[1024];
            snprintf(url, 1024, "https://www.google-analytics.com/mp/collect?api_secret=%s&measurement_id=%s", UA_API_SECRET, UA_MEASUREMENT_ID);
            curl_easy_setopt(curl, CURLOPT_URL, url);

            char post_data[1024];
            snprintf(post_data, 1024, "{\"client_id\": \"%s\", \"events\": [{\"name\": \"%s\"}]}", UA_CLIENT_ID, event_name);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

            /* Perform the request, res will get the return code */
            res = curl_easy_perform(curl);

            /* always cleanup */
            curl_easy_cleanup(curl);
        }
    #endif
};

// Distill all possible modes into one enum. It's spread across multiple configs, so it's not a perfect representation.
enum metrics_output_mode {
    OUTPUT_MODE_DISABLED,
    OUTPUT_MODE_SIDEVIEW,
    OUTPUT_MODE_VIRTUAL_DISPLAY,
    OUTPUT_MODE_MOUSE,
    OUTPUT_MODE_JOYSTICK
};

char *metrics_output_mode_to_event_name[5] = {
    "output_mode_disabled",
    "output_mode_follow",
    "output_mode_virtual_display",
    "output_mode_mouse",
    "output_mode_joystick"
};

enum metrics_output_mode current_output_mode = OUTPUT_MODE_DISABLED;
bool config_disabled = true;
char *config_output_mode = NULL;
char *config_external_mode = NULL;
char *current_device = NULL;
bool state_sbs_enabled = false;
bool was_smooth_follow_enabled = false;
bool config_smooth_follow_enabled = false;
bool was_auto_recenter_enabled = false;
bool config_auto_recenter_enabled = false;

void metrics_handle_config_line_func(void* config, char* key, char* value) {
    if (!config_output_mode) config_output_mode = strdup(mouse_output_mode);
    if (!config_external_mode) config_external_mode = strdup("none");
    if (equal(key, "output_mode")) {
        string_config(key, value, &config_output_mode);
    } else if (equal(key, "external_mode")) {
        string_config(key, value, &config_external_mode);
    } else if (equal(key, "disabled")) {
        boolean_config(key, value, &config_disabled);
    } else if (equal(key, "virtual_display_smooth_follow_enabled")) {
        boolean_config(key, value, &config_auto_recenter_enabled);
    } else if (equal(key, "sideview_smooth_follow_enabled")) {
        boolean_config(key, value, &config_smooth_follow_enabled);
    }
};

void metrics_set_config_func(void* config) {
    enum metrics_output_mode new_output_mode = OUTPUT_MODE_DISABLED;
    if (!config_disabled) {
        if (!config_output_mode || equal(config_output_mode, mouse_output_mode)) {
            new_output_mode = OUTPUT_MODE_MOUSE;
        } else if (equal(config_output_mode, joystick_output_mode)) {
            new_output_mode = OUTPUT_MODE_JOYSTICK;
        } else if (equal(config_external_mode, "virtual_display")) {
            new_output_mode = OUTPUT_MODE_VIRTUAL_DISPLAY;
        } else if (equal(config_external_mode, "sideview")) {
            new_output_mode = OUTPUT_MODE_SIDEVIEW;
        }
    }

    if (device_present()) {
        bool output_mode_changed = current_output_mode != new_output_mode;
        if (output_mode_changed && new_output_mode != OUTPUT_MODE_DISABLED) {
            log_metric(metrics_output_mode_to_event_name[new_output_mode]);
        }
        current_output_mode = new_output_mode;

        if (config_smooth_follow_enabled && (!was_smooth_follow_enabled ||
                output_mode_changed && new_output_mode == OUTPUT_MODE_SIDEVIEW)) {
            log_metric("smooth_follow_enabled");
        }
        was_smooth_follow_enabled = config_smooth_follow_enabled;

        if (config_auto_recenter_enabled && (!was_auto_recenter_enabled ||
                output_mode_changed && new_output_mode == OUTPUT_MODE_VIRTUAL_DISPLAY)) {
            log_metric("auto_recenter_enabled");
        }
        was_auto_recenter_enabled = config_auto_recenter_enabled;
    }
};


void metrics_handle_state_func() {
    if (!state_sbs_enabled && state()->sbs_mode_enabled) {
        log_metric("sbs_enabled");
        state_sbs_enabled = state()->sbs_mode_enabled;
    }
};

void metrics_handle_device_connect_func() {
    device_properties_type* device = device_checkout();
    if (device != NULL) {
        char new_device[1024];
        snprintf(new_device, 1024, "device_%x_%x", device->hid_vendor_id, device->hid_product_id);

        if (!current_device || strcmp(new_device, current_device) != 0) {
            log_metric(new_device);
            free_and_clear(&current_device);
            current_device = strdup(new_device);
        }
    }
    device_checkin(device);
};

const plugin_type metrics_plugin = {
    .id = "metrics",
    .handle_config_line = metrics_handle_config_line_func,
    .set_config = metrics_set_config_func,
    .handle_state = metrics_handle_state_func,
    .handle_device_connect = metrics_handle_device_connect_func
};