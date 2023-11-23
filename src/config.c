#include "config.h"
#include "device.h"
#include "string.h"

#include <stdbool.h>
#include <stdio.h>

const char *joystick_output_mode = "joystick";
const char *mouse_output_mode = "mouse";
const char *external_only_output_mode = "external_only";


driver_config_type *default_config() {
    driver_config_type *config = malloc(sizeof(driver_config_type));
    if (config == NULL) {
        fprintf(stderr, "Error allocating config");
        exit(1);
    }

    config->disabled = false;
    config->use_roll_axis = false;
    config->mouse_sensitivity = 30;
    config->output_mode = NULL;
    config->look_ahead_override = 0.0;
    config->external_zoom = 1.0;
    config->debug_threads = false;
    config->debug_joystick = false;
    config->debug_multi_tap = false;
    config->debug_ipc = false;

    copy_string(mouse_output_mode, &config->output_mode, strlen(mouse_output_mode) + 1);

    return config;
}

void update_config(driver_config_type **config, driver_config_type *new_config) {
    free((*config)->output_mode);
    free(*config);
    *config = new_config;
}

bool is_joystick_mode(driver_config_type *config) {
    return strcmp(config->output_mode, joystick_output_mode) == 0;
}

bool is_mouse_mode(driver_config_type *config) {
    return strcmp(config->output_mode, mouse_output_mode) == 0;
}

bool is_external_mode(driver_config_type *config) {
    return strcmp(config->output_mode, external_only_output_mode) == 0;
}

bool is_evdev_output_mode(driver_config_type *config) {
    return is_mouse_mode(config) || is_joystick_mode(config);
}