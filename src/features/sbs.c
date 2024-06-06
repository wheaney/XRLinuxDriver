#include "runtime_context.h"
#include "strings.h"

const char* sbs_feature_name = "sbs";

bool is_sbs_granted() {
    return state() && state()->granted_features && state()->granted_features_count &&
           in_array(sbs_feature_name, (const char**)state()->granted_features, state()->granted_features_count);
}