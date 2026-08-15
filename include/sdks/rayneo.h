#ifndef XR_MINI_SDK_H
#define XR_MINI_SDK_H

#include "base/FXRError.h"
#include "base/FXRMacro.h"
#include "device/usb/XRDeviceState.h"

#include <stdbool.h>
#include <stdint.h>

// libRayNeoXRMiniSDK.so statically links libusb and re-exports ~100 libusb_* symbols with default
// visibility. Linking it puts those ahead of the real libusb-1.0.so.0 in the global scope, so other
// libraries bind to RayNeo's copy: VITURE's libcarina_vio.so then hands a libusb_context to the
// wrong implementation and segfaults in libusb_ref_device, with no RayNeo device attached at all.
//
// So it's dlopen'd on demand instead. RTLD_LOCAL keeps its libusb_* out of the global scope, so
// nothing else can bind to them whatever the hot-plug order. RTLD_DEEPBIND keeps the blob's own
// libusb calls (interposable via its PLT) on its bundled copy, which would otherwise fail in
// reverse. Entry points below are therefore dlsym'd pointers; call rayneo_sdk_load() first.
bool rayneo_sdk_load(void);

extern void (*RegisterIMUEventCallback)(IMUEventCallback callback);

extern void (*UnregisterIMUEventCallback)(IMUEventCallback callback);

extern void (*RegisterStateEventCallback)(StateEventCallback callback);

extern void (*UnregisterStateEventCallback)(StateEventCallback callback);

extern int (*EstablishUsbConnection)(int32_t vid, int32_t pid);

extern int (*ResetUsbConnection)();

extern void (*NotifyDeviceConnected)();

extern void (*NotifyDeviceDisconnected)();

extern void (*StartXR)();

extern void (*StopXR)();

extern void (*SwitchTo2D)();

extern void (*SwitchTo3D)();

extern void (*OpenIMU)();

extern void (*CloseIMU)();

extern void (*Recenter)();

extern void (*GetHeadTrackerPose)(float rotation[4], float position[3], uint64_t* timeNsInDevice);

extern uint64_t (*ConvertHostTime2DeviceTime)(uint64_t timeNsInHost);

extern void (*GetDeviceType)(char* device);

extern void (*AcquireDeviceInfo)();

extern int8_t (*GetSideBySideStatus)();
#endif
