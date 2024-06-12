#ifndef FFALCONXR_DEBUG_H
#define FFALCONXR_DEBUG_H

#ifndef LOG_TAG
#define LOG_TAG "RayNeoXR"
#endif

#ifdef ANDROID

#include <android/log.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NDEBUG
#define FXR_LOG(LEVEL, format, ...)                                                                                    \
    ((void) __android_log_print(ANDROID_LOG_##LEVEL, LOG_TAG, "[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__))
#else
#define FXR_LOG(LEVEL, format, ...) ((void) __android_log_print(ANDROID_LOG_##LEVEL, LOG_TAG, format, ##__VA_ARGS__))

#ifndef LOGD
#define LOGD(format, ...)
#endif
#endif

#define FXR_LOG_FATAL(condition, format, ...) ((void) __android_log_assert(condition, LOG_TAG, format, ##__VA_ARGS__))
#define FXR_CHECK(condition, format, ...)                                                                              \
    (__predict_false(condition)) ? FXR_LOG_FATAL(condition, format, ##__VA_ARGS__) : ((void)0))


#ifndef LOGD
#define LOGD(format, ...) FXR_LOG(DEBUG, format, ##__VA_ARGS__)
#endif
#ifndef LOGI
#define LOGI(format, ...) FXR_LOG(INFO, format, ##__VA_ARGS__)
#endif
#ifndef LOGW
#define LOGW(format, ...) FXR_LOG(WARN, format, ##__VA_ARGS__)
#endif
#ifndef LOGE
#define LOGE(format, ...) FXR_LOG(ERROR, format, ##__VA_ARGS__)
#endif
#ifndef LOG_FATAL
#define LOG_FATAL(format, ...) FXR_LOG_FATAL(NULL, format, ##__VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#else
#include "stdio.h"

#define FXR_LOG(LEVEL, format, ...) printf("[%s]" format "\r\n", LEVEL, ##__VA_ARGS__)
#ifndef LOGD
#define LOGD(format, ...) FXR_LOG("DEBUG", format, ##__VA_ARGS__)
#endif
#ifndef LOGI
#define LOGI(format, ...) FXR_LOG("INFO", format, ##__VA_ARGS__)
#endif
#ifndef LOGW
#define LOGW(format, ...) FXR_LOG("WARN", format, ##__VA_ARGS__)
#endif
#ifndef LOGE
#define LOGE(format, ...) FXR_LOG("ERROR", format, ##__VA_ARGS__)
#endif

#endif // ANDROID

#include <stdint.h>

extern const char* GetEventStateName(int32_t state);
extern int32_t GetFFalconErrorCode(int32_t libusbErrCode);

#endif //FFALCONXR_DEBUG_H
