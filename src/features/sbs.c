#include "runtime_context.h"
#include "strings.h"

#include <stdatomic.h>

const char* sbs_feature_name = "sbs";

static atomic_int sbs_granted_cached = ATOMIC_VAR_INIT(-1);

void reset_sbs_features() {
    atomic_store_explicit(&sbs_granted_cached, -1, memory_order_release);
}

bool is_sbs_granted() {
    int cached = atomic_load_explicit(&sbs_granted_cached, memory_order_acquire);
    if (cached != -1) return cached;

    driver_state_type* s = state();
    bool granted = s && s->granted_features && s->granted_features_count &&
                   in_array(sbs_feature_name, (const char**)s->granted_features, s->granted_features_count);

    atomic_store_explicit(&sbs_granted_cached, granted ? 1 : 0, memory_order_release);
    return granted;
}