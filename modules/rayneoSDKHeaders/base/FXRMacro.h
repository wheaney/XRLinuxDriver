#ifndef FFALCONXR_MACR_H
#define FFALCONXR_MACR_H

#include "base/FXRError.h"
#include <stddef.h>
#include <stdint.h>
#ifdef ANDROID
#include <android/hardware_buffer.h>
#endif

typedef void (*IMUEventCallback)(const float acc[3], const float gyro[3], const float mag[3], uint64_t timeInNs);
typedef void (*VsyncEventCallback)();
typedef void (*StateEventCallback)(uint32_t state, uint64_t timestamp, size_t length, const void* data);
typedef void* (*SensorFusionFactory)();
typedef void (*XRConfigBroadcastCallback)();
typedef FXRResult (*XRConfigExternalReader)(const char* item, void*);
#ifdef  ANDROID
typedef void (*FrameEventCallback)(AHardwareBuffer* buffer, int64_t timestamp, int64_t exposureTime);
#else
typedef void (*FrameEventCallback)(void* buffer, int64_t timestamp, int64_t exposureTime);
#endif
typedef void (*FrameDataCallback)(uint8_t* data, uint32_t w, uint32_t h, int64_t timestamp, int64_t exposureTime);

#define XR_CHARACTER_ARRAYS_MAX_LEN (256)
#define XR_PLANE_ARRAY_DEFAULT_SIZE (15)

#if defined(__CYGWIN32__)
#define RAYNEO_API_CALL __stdcall
#define RAYNEO_API_EXPORT __declspec(dllexport)
#define RAYNEO_API_PTR RAYNEO_API_CALL
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || defined(WINAPI_FAMILY)
#define RAYNEO_API_CALL __stdcall
#define RAYNEO_API_EXPORT __declspec(dllexport)
#define RAYNEO_API_PTR RAYNEO_API_CALL
#elif defined(__MACH__) || defined(__ANDROID__) || defined(__linux__) || defined(LUMIN)
#define RAYNEO_API_CALL
#define RAYNEO_API_EXPORT __attribute__((visibility("default")))
#define RAYNEO_API_PTR
#else
#define RAYNEO_API_CALL
#define RAYNEO_API_EXPORT
#define RAYNEO_API_PTR
#endif

#define AUTO_GENERATED

#endif //FFALCONXR_MACR_H
