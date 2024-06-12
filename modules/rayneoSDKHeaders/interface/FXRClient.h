#ifndef FFALCONXR_CLIENT_H
#define FFALCONXR_CLIENT_H

#include "interface/FXRApi.h"
#include <android/native_window.h>

extern "C" {


RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL EstablishServiceConnection(int32_t fd);

RAYNEO_API_EXPORT void RAYNEO_API_CALL ReleaseServiceConnection();

RAYNEO_API_EXPORT void RAYNEO_API_CALL NativeVsync(int64_t frameTimeNanos, float displayRefreshRateHz);

RAYNEO_API_EXPORT void RAYNEO_API_CALL LoadConfigFromMemory(const uint8_t* buf, const uint32_t len);

RAYNEO_API_EXPORT void RAYNEO_API_CALL SetSurface(ANativeWindow* window, int32_t width, int32_t height);

RAYNEO_API_EXPORT void RAYNEO_API_CALL SetWindowFocus(bool hasFocus);
}


#endif //FFALCONXR_CLIENT_H
