#include "runtime_context.h"
#include "strings.h"

const char* sbs_feature_name = "sbs";

bool is_sbs_granted() {
    return context.state && context.state->granted_features && context.state->granted_features_count &&
           in_array(sbs_feature_name, context.state->granted_features, context.state->granted_features_count);
}