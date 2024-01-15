#include "runtime_context.h"
#include "strings.h"

const char* smooth_follow_feature_name = "smooth_follow";

bool is_smooth_follow_granted() {
    return context.state && context.state->granted_features && context.state->granted_features_count &&
           in_array(smooth_follow_feature_name, context.state->granted_features, context.state->granted_features_count);
}