#include "custom_banner_config.h"
#include "plugins/custom_banner.h"
#include "ipc.h"
#include "runtime_context.h"

#include <stdlib.h>
#include <time.h>

const uint32_t banner_start_date = CUSTOM_BANNER_START_DATE;
const uint32_t banner_end_date = CUSTOM_BANNER_END_DATE;
const int target_device_vendor_id = CUSTOM_BANNER_TARGET_DEVICE_VENDOR_ID;
const int target_device_product_id = CUSTOM_BANNER_TARGET_DEVICE_PRODUCT_ID;

custom_banner_ipc_values_type *custom_banner_ipc_values;
void evaluate_banner_conditions() {
    device_properties_type* device = device_checkout();
    if (custom_banner_ipc_values && device != NULL) {
        bool any_conditions_set = false;
        bool banner_device_conditions_met = true;
        if (target_device_vendor_id != 0) {
            any_conditions_set = true;
            banner_device_conditions_met = device->hid_vendor_id == target_device_vendor_id;
            if (target_device_product_id != 0) {
                banner_device_conditions_met &= device->hid_product_id == target_device_product_id;
            }
        }

        bool banner_time_conditions_met = true;
        time_t now = time(NULL);
        if (banner_start_date != 0) {
            any_conditions_set = true;
            banner_time_conditions_met = now > banner_start_date;
        }
        if (banner_end_date != 0) {
            any_conditions_set = true;
            banner_time_conditions_met &= now < banner_end_date;
        }

        *custom_banner_ipc_values->enabled = any_conditions_set && banner_device_conditions_met && banner_time_conditions_met;
    }
    device_checkin(device);
}

const char *custom_banner_enabled_name = "custom_banner_enabled";

bool custom_banner_setup_ipc_func() {
    bool debug = config()->debug_ipc;
    if (!custom_banner_ipc_values) custom_banner_ipc_values = calloc(1, sizeof(custom_banner_ipc_values_type));
    setup_ipc_value(custom_banner_enabled_name, (void**) &custom_banner_ipc_values->enabled, sizeof(bool), debug);

    evaluate_banner_conditions();

    return true;
}

void custom_banner_handle_device_connect_func() {
    evaluate_banner_conditions();
};

void custom_banner_reset_imu_data_func() {
    evaluate_banner_conditions();
};

const plugin_type custom_banner_plugin = {
    .id = "custom_banner",
    .setup_ipc = custom_banner_setup_ipc_func,
    .reset_imu_data = custom_banner_reset_imu_data_func,
    .handle_device_connect = custom_banner_handle_device_connect_func
};