#include <stdbool.h>

extern const char* productivity_basic_feature_name;
extern const char* productivity_pro_feature_name;

bool is_productivity_basic_granted();
bool is_productivity_pro_granted();
bool is_productivity_granted();
void reset_productivity_features();