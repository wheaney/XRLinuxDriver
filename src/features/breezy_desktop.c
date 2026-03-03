#include "runtime_context.h"
#include "strings.h"

#include <stdatomic.h>

const char* productivity_basic_feature_name = "productivity";
const char* productivity_pro_feature_name = "productivity_pro";

static atomic_int productivity_basic_granted_cached = ATOMIC_VAR_INIT(-1);
static atomic_int productivity_pro_granted_cached = ATOMIC_VAR_INIT(-1);
static atomic_int productivity_granted_cached = ATOMIC_VAR_INIT(-1);

void reset_productivity_features() {
    atomic_store_explicit(&productivity_basic_granted_cached, -1, memory_order_release);
    atomic_store_explicit(&productivity_pro_granted_cached, -1, memory_order_release);
    atomic_store_explicit(&productivity_granted_cached, -1, memory_order_release);
}

bool is_productivity_basic_granted() {
    int cached = atomic_load_explicit(&productivity_basic_granted_cached, memory_order_acquire);
    if (cached != -1) return cached;

    driver_state_type* s = state();
    bool granted = s && s->granted_features && s->granted_features_count &&
                   in_array(productivity_basic_feature_name, (const char**)s->granted_features, s->granted_features_count);

    atomic_store_explicit(&productivity_basic_granted_cached, granted ? 1 : 0, memory_order_release);
    return granted;
}

bool is_productivity_pro_granted() {
    int cached = atomic_load_explicit(&productivity_pro_granted_cached, memory_order_acquire);
    if (cached != -1) return cached;

    driver_state_type* s = state();
    bool granted = s && s->granted_features && s->granted_features_count &&
                   in_array(productivity_pro_feature_name, (const char**)s->granted_features, s->granted_features_count);

    atomic_store_explicit(&productivity_pro_granted_cached, granted ? 1 : 0, memory_order_release);
    return granted;
}

bool is_productivity_granted() {
    int cached = atomic_load_explicit(&productivity_granted_cached, memory_order_acquire);
    if (cached != -1) return cached;

    bool granted = is_productivity_basic_granted() || is_productivity_pro_granted();
    atomic_store_explicit(&productivity_granted_cached, granted ? 1 : 0, memory_order_release);
    return granted;
}