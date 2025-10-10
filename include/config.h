#pragma once

#include <stdbool.h>
#include <stdio.h>

struct driver_config_t {
    bool disabled;
    bool mouse_mode;
    bool joystick_mode;
    bool external_mode;
    bool use_roll_axis;
    bool vr_lite_invert_x;
    bool vr_lite_invert_y;
    int mouse_sensitivity;
    char *output_mode;
    bool multi_tap_enabled;
    bool metrics_disabled;

    bool debug_threads;
    bool debug_joystick;
    bool debug_multi_tap;
    bool debug_ipc;
    bool debug_license;
    bool debug_device;
    bool debug_connections;
};

typedef struct driver_config_t driver_config_type;

extern const char *joystick_output_mode;
extern const char *mouse_output_mode;
extern const char *external_only_output_mode;

driver_config_type *default_config();
void update_config(driver_config_type *config, driver_config_type *new_config);
driver_config_type* parse_config_file(FILE *fp);

void boolean_config(char* key, char *value, bool *config_value);
void float_config(char* key, char *value, float *config_value);
void int_config(char* key, char *value, int *config_value);
void string_config(char* key, char *value, char **config_value);