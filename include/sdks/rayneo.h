#ifndef XR_MINI_SDK_H
#define XR_MINI_SDK_H

#include "base/FXRError.h"
#include "base/FXRMacro.h"
#include "device/usb/XRDeviceState.h"

void RegisterIMUEventCallback(IMUEventCallback callback);

void UnregisterIMUEventCallback(IMUEventCallback callback);

void RegisterStateEventCallback(StateEventCallback callback);

void UnregisterStateEventCallback(StateEventCallback callback);

int EstablishUsbConnection(int32_t vid, int32_t pid);

int ResetUsbConnection();

void NotifyDeviceConnected();

void NotifyDeviceDisconnected();

void StartXR();

void StopXR();

void SwitchTo2D();

void SwitchTo3D();

void OpenIMU();

void CloseIMU();

void Recenter();

void GetHeadTrackerPose(float rotation[4], float position[3], uint64_t* timeNsInDevice);

uint64_t ConvertHostTime2DeviceTime(uint64_t timeNsInHost);

void GetDeviceType(char* device);

void AcquireDeviceInfo();

int8_t GetSideBySideStatus();
#endif