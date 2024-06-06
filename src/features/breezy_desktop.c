#include "runtime_context.h"
#include "strings.h"

const char* productivity_basic_feature_name = "productivity_basic";
const char* productivity_pro_feature_name = "productivity_pro";

bool is_productivity_basic_granted() {
    return state() && state()->granted_features && state()->granted_features_count &&
           in_array(productivity_basic_feature_name, (const char**)state()->granted_features, state()->granted_features_count);
}

bool is_productivity_pro_granted() {
    return state() && state()->granted_features && state()->granted_features_count &&
           in_array(productivity_pro_feature_name, (const char**)state()->granted_features, state()->granted_features_count);
}

bool is_productivity_granted() {
    return is_productivity_basic_granted() || is_productivity_pro_granted();
}