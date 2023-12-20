#pragma once

#include <stdbool.h>
#include <stdio.h>

struct driver_config_t {
    bool disabled;
    bool use_roll_axis;
    int mouse_sensitivity;
    char *output_mode;
    float look_ahead_override;
    float display_zoom;
    float display_distance;

    bool sbs_content;
    bool sbs_mode_stretched;

    bool debug_threads;
    bool debug_joystick;
    bool debug_multi_tap;
    bool debug_ipc;
};

typedef struct driver_config_t driver_config_type;

extern const char *joystick_output_mode;
extern const char *mouse_output_mode;
extern const char *external_only_output_mode;

driver_config_type *default_config();
void update_config(driver_config_type **config, driver_config_type *new_config);
driver_config_type* parse_config_file(FILE *fp);

bool equal(char *key, const char *desired_key);
void boolean_config(char* key, char *value, bool *config_value);
void float_config(char* key, char *value, float *config_value);
void int_config(char* key, char *value, int *config_value);
void string_config(char* key, char *value, char **config_value);

bool is_joystick_mode(driver_config_type *config);
bool is_mouse_mode(driver_config_type *config);
bool is_external_mode(driver_config_type *config);
bool is_evdev_output_mode(driver_config_type *config);