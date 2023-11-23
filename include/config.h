#pragma once

#include <stdbool.h>

struct driver_config_t {
    bool disabled;
    bool use_roll_axis;
    int mouse_sensitivity;
    char *output_mode;
    float look_ahead_override;
    float external_zoom;

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

bool is_joystick_mode(driver_config_type *config);
bool is_mouse_mode(driver_config_type *config);
bool is_external_mode(driver_config_type *config);
bool is_evdev_output_mode(driver_config_type *config);