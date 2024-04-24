#ifndef FFALCONXR_SERVER_H
#define FFALCONXR_SERVER_H

#include "interface/FXRApi.h"


extern "C" {


RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL EstablishUsbConnection(int32_t fileDescriptor);

RAYNEO_API_EXPORT void RAYNEO_API_CALL ReleaseUsbConnection();

RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL AcceptConnection(int32_t fd);

RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL LoadConfigFromStorage(const char* path);

RAYNEO_API_EXPORT void RAYNEO_API_CALL SetSensorFusionFactory(SensorFusionFactory factory);

RAYNEO_API_EXPORT void RAYNEO_API_CALL RegisterConfigChangesNotifier(XRConfigBroadcastCallback notifier);
}

#endif //FFALCONXR_SERVER_H
