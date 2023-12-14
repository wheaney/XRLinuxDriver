#include "config.h"
#include "device.h"
#include "string.h"

#include <errno.h>
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
    config->display_zoom = 1.0;
    config->display_distance = 1.0;
    config->debug_threads = false;
    config->debug_joystick = false;
    config->debug_multi_tap = false;
    config->debug_ipc = false;

    copy_string(mouse_output_mode, &config->output_mode);

    return config;
}

void update_config(driver_config_type **config, driver_config_type *new_config) {
    free((*config)->output_mode);
    free(*config);
    *config = new_config;
}

driver_config_type* parse_config_file(FILE *fp) {
    driver_config_type *config = default_config();

    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        if (strcmp(key, "disabled") == 0) {
            config->disabled = strcmp(value, "true") == 0;
        } else if (strcmp(key, "debug") == 0) {
            char *token = strtok(value, ",");
            while (token != NULL) {
                if (strcmp(token, "joystick") == 0) {
                    config->debug_joystick = true;
                }
                if (strcmp(token, "taps") == 0) {
                    config->debug_multi_tap = true;
                }
                if (strcmp(token, "threads") == 0) {
                    config->debug_threads = true;
                }
                if (strcmp(token, "ipc") == 0) {
                    config->debug_ipc = true;
                }
                token = strtok(NULL, ",");
            }
        } else if (strcmp(key, "use_roll_axis") == 0) {
            config->use_roll_axis = true;
        } else if (strcmp(key, "mouse_sensitivity") == 0) {
            char *endptr;
            errno = 0;
            long num = strtol(value, &endptr, 10);
            if (errno != ERANGE && endptr != value) {
                config->mouse_sensitivity = (int) num;
            } else {
                fprintf(stderr, "Error parsing mouse_sensitivity value: %s\n", value);
            }
        } else if (strcmp(key, "look_ahead") == 0) {
            char *endptr;
            errno = 0;
            float num = strtof(value, &endptr);
            if (errno != ERANGE && endptr != value) {
                config->look_ahead_override = num;
            } else {
                fprintf(stderr, "Error parsing look_ahead value: %s\n", value);
            }
        } else if (strcmp(key, "external_zoom") == 0 || strcmp(key, "display_zoom") == 0) {
             char *endptr;
             errno = 0;
             float num = strtof(value, &endptr);
             if (errno != ERANGE && endptr != value) {
                 config->display_zoom = num;
             } else {
                 fprintf(stderr, "Error parsing %s value: %s\n", key, value);
             }
         } else if (strcmp(key, "display_distance") == 0) {
              char *endptr;
              errno = 0;
              float num = strtof(value, &endptr);
              if (errno != ERANGE && endptr != value) {
                  config->display_distance = num;
              } else {
                  fprintf(stderr, "Error parsing display_distance value: %s\n", value);
              }
          } else if (strcmp(key, "output_mode") == 0) {
            copy_string(value, &config->output_mode);
        }
    }

    return config;
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