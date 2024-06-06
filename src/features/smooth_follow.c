#include "runtime_context.h"
#include "strings.h"

const char* smooth_follow_feature_name = "smooth_follow";

bool is_smooth_follow_granted() {
    return state() && state()->granted_features && state()->granted_features_count &&
           in_array(smooth_follow_feature_name, (const char**)state()->granted_features, state()->granted_features_count);
}