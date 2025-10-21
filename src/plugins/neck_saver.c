#include "plugins/neck_saver.h"
#include "imu.h"
#include "logging.h"
#include "memory.h"
#include "strings.h"
#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static neck_saver_config_type *ns_config = NULL;

static void *neck_saver_default_config_func() {
    neck_saver_config_type *config = calloc(1, sizeof(neck_saver_config_type));
    config->horizontal_multiplier = 1.0f;
    config->vertical_multiplier = 1.0f;
    return config;
}

static void neck_saver_handle_config_line_func(void *config, char *key, char *value) {
    neck_saver_config_type *c = (neck_saver_config_type*)config;
    if (equal(key, "neck_saver_horizontal_multiplier")) {
        float_config(key, value, &c->horizontal_multiplier);
    } else if (equal(key, "neck_saver_vertical_multiplier")) {
        float_config(key, value, &c->vertical_multiplier);
    }
}

static void neck_saver_set_config_func(void *config) {
    neck_saver_config_type *new_config = (neck_saver_config_type*)config;
    if (!ns_config) {
        ns_config = new_config;
        return;
    }

    if (ns_config->horizontal_multiplier != new_config->horizontal_multiplier) {
        log_message("Neck Saver horizontal multiplier changed to %f\n", new_config->horizontal_multiplier);
    }
    if (ns_config->vertical_multiplier != new_config->vertical_multiplier) {
        log_message("Neck Saver vertical multiplier changed to %f\n", new_config->vertical_multiplier);
    }

    free(ns_config);
    ns_config = new_config;
}

static void neck_saver_modify_pose_func(imu_pose_type* pose) {
    if (!ns_config || !pose) return;
    if (ns_config->horizontal_multiplier == 1.0f && ns_config->vertical_multiplier == 1.0f) return;

    // euler is XYZ (roll,pitch,yaw). Apply multipliers accordingly.
    pose->euler.yaw *= ns_config->horizontal_multiplier;
    pose->euler.pitch *= ns_config->vertical_multiplier;

    // Keep quaternion consistent with updated euler angles
    pose->orientation = euler_to_quaternion_zyx(pose->euler);
}

const plugin_type neck_saver_plugin = {
    .id = "neck_saver",
    .default_config = neck_saver_default_config_func,
    .handle_config_line = neck_saver_handle_config_line_func,
    .set_config = neck_saver_set_config_func,
    .modify_pose = neck_saver_modify_pose_func
};
