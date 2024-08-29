#include <stdbool.h>
#include <stddef.h>

typedef void (*gamescope_reshade_effect_ready_callback)();

bool gamescope_reshade_wl_server_connect();

void gamescope_reshade_wl_server_disconnect();

bool enable_gamescope_virtual_display_effect();

void disable_gamescope_virtual_display_effect();

void set_gamescope_reshade_effect_uniform_variable(const char *variable_name, const void *data, int entries, size_t size, bool flush);

void add_gamescope_effect_ready_listener(gamescope_reshade_effect_ready_callback callback);