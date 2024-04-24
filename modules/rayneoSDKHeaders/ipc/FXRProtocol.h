#ifndef FFALCONXR_IPC_PROTOCOL_H
#define FFALCONXR_IPC_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif


enum FXRStateEvent {
    kStateUnknown = 0,
    kStateDeviceConnected,
    kStateDeviceDisconnected,
    kStateImuOn,
    kStateImuOff,
    kStateVolumeChanged,
    kStateWheelKeyPressed,
    kStateWheelKeyLongPressed,
    kStateWheelKeyDoublePressed,
    kStateDeviceInterrupt,
    kStateDevicePlugged,
    kStateDeviceUnPlugged,
    kStatePanelLunaChanged,
    kStateAudioModeChanged,
    kStateGlassSleep,
    kStateGlassWakeup,


    //response
    kStateDeviceInfoResponseArrived = 0x4000,


    //Mag Calibration
    kStateMagNeedCalibrate = 0x6000,
    kStateMagDoingCalibrate = 0x6001,
    kStateMagCalibrateSuccess = 0x6002,
    kStateMagCalibrateFailed = 0x6003,

    //time sync
    kStateSynchronizeTime = 0x8001,
};

//注意与FxrApi.java中的定义保持一致
enum CallbackType {
    IMUEvent = 0,
    StateEvent = 1,
    VsyncEvent = 2,
    FrameEvent = 3,
};

enum FXRControlUnit {
    kUnitUnknown,
    kUnitHeadTracker,
    kUnitConfiguration,
};

enum FXRControlCommand {
    kCtlCmdUnknown,
    kCtlCmdStartMagCalibration,
    kCtlCmdStopMagCalibration,
    kCtlCmdResetRenderParameters,
    kCtlCmdUseATW,
    kCtlCmdUseSinglePass,
};


#ifdef __cplusplus
}
#endif

#endif //FFALCONXR_IPC_PROTOCOL_H
