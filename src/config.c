#include "config.h"
#include "logging.h"
#include "memory.h"
#include "plugins.h"
#include "strings.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

const char *joystick_output_mode = "joystick";
const char *mouse_output_mode = "mouse";
const char *external_only_output_mode = "external_only";

driver_config_type *default_config() {
    driver_config_type *config = calloc(1, sizeof(driver_config_type));
    if (config == NULL) {
        log_error("Error allocating config");
        exit(1);
    }

    config->disabled = true;
    config->mouse_mode = false;
    config->joystick_mode = false;
    config->external_mode = false;
    config->use_roll_axis = false;
    config->vr_lite_invert_x = false;
    config->vr_lite_invert_y = false;
    config->mouse_sensitivity = 30;
    config->output_mode = strdup(mouse_output_mode);
    config->multi_tap_enabled = false;
    config->metrics_disabled = false;
    config->dead_zone_threshold_deg = 0.0f;

    config->debug_threads = false;
    config->debug_joystick = false;
    config->debug_multi_tap = false;
    config->debug_ipc = false;
    config->debug_license = false;
    config->debug_device = false;
    config->debug_connections = false;

    return config;
}

void update_config(driver_config_type *config, driver_config_type *new_config) {
    free(config->output_mode);
    *config = *new_config;
    free(new_config);
}

void boolean_config(char* key, char *value, bool *config_value) {
    *config_value = equal(value, "true");
}

void float_config(char* key, char *value, float *config_value) {
    char *endptr;
    errno = 0;
    float num = strtof(value, &endptr);
    if (errno != ERANGE && endptr != value) {
        *config_value = num;
    } else {
        log_error("Error parsing %s value: %s\n", key, value);
    }
}

void int_config(char* key, char *value, int *config_value) {
    char *endptr;
    errno = 0;
    long num = strtol(value, &endptr, 10);
    if (errno != ERANGE && endptr != value) {
        *config_value = (int) num;
    } else {
        log_error("Error parsing %s value: %s\n", key, value);
    }
}

void string_config(char* key, char *value, char **config_value) {
    free_and_clear(config_value);
    *config_value = strdup(value);
}

driver_config_type* parse_config_file(FILE *fp) {
    driver_config_type *config = default_config();
    void *plugin_configs = plugins.default_config();

    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        if (equal(key, "disabled")) {
            boolean_config(key, value, &config->disabled);
        } else if (equal(key, "debug")) {
            char *token = strtok(value, ",");
            while (token != NULL) {
                if (equal(token, "joystick")) {
                    config->debug_joystick = true;
                }
                if (equal(token, "taps")) {
                    config->debug_multi_tap = true;
                }
                if (equal(token, "threads")) {
                    config->debug_threads = true;
                }
                if (equal(token, "ipc")) {
                    config->debug_ipc = true;
                }
                if (equal(token, "license")) {
                    config->debug_license = true;
                }
                if (equal(token, "device")) {
                    config->debug_device = true;
                }
                if (equal(token, "connections")) {
                    config->debug_connections = true;
                }
                token = strtok(NULL, ",");
            }
        } else if (equal(key, "use_roll_axis")) {
            config->use_roll_axis = true;
        } else if (equal(key, "vr_lite_invert_x")) {
            boolean_config(key, value, &config->vr_lite_invert_x);
        } else if (equal(key, "vr_lite_invert_y")) {
            boolean_config(key, value, &config->vr_lite_invert_y);
        } else if (equal(key, "mouse_sensitivity")) {
            int_config(key, value, &config->mouse_sensitivity);
        } else if (equal(key, "output_mode")) {
            string_config(key, value, &config->output_mode);
            config->joystick_mode = strcmp(config->output_mode, joystick_output_mode) == 0;
            config->mouse_mode = strcmp(config->output_mode, mouse_output_mode) == 0;
            config->external_mode = strcmp(config->output_mode, external_only_output_mode) == 0;
        } else if (equal(key, "multi_tap_enabled")) {
            boolean_config(key, value, &config->multi_tap_enabled);
        } else if (equal(key, "metrics_disabled")) {
            boolean_config(key, value, &config->metrics_disabled);
        } else if (equal(key, "dead_zone_threshold_deg")) {
            float_config(key, value, &config->dead_zone_threshold_deg);
        }

        plugins.handle_config_line(plugin_configs, key, value);
    }
    plugins.set_config(plugin_configs);

    return config;
}