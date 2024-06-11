#pragma once

#include "base/FXRType.h"

#define FXR_HID_SEND_MAGIC 0x66
#define FXR_HID_ACK_MAGIC 0x99

#ifdef __cplusplus
extern "C" {
#endif

enum FXRColorAdjustOp {
    opWhiteBalanceWx = 0x01,
    opWhiteBalanceWy = 0x02,
    opColorGainR = 0x03,
    opColorGainG = 0x04,
    opColorGainB = 0x05,
    opColorEnhancementR = 0x06,
    opColorEnhancementG = 0x07,
    opColorEnhancementB = 0x08,
    opContrast = 0x09,
    opColorTemperature = 0x0A,
    opHue = 0x0B,
    opLoad = 0xF0,
    opReset = 0xFE,
    opSave = 0xFF,
};


//kCmd* 为双向命令
//kAck* 为单向命令, 眼镜端->设备端
enum FXRUsbCommand {
    kCmdDeviceInfo = 0,
    kCmdImuOn = 1,
    kCmdImuOff = 2,
    kCmdSensorGyroCorrection = 3,
    kCmdDisplay3dMode = 6,
    kCmdDisplay2dMode = 7,
    kCmdPanelPresetSet = 9,
    kCmdSetPanelRotation = 0xA,
    kCmdPanelLumaSave = 0xD,
    kCmdPanelPowerOn = 0xE,
    kCmdPanelPowerOff = 0xF,
    kCmdPanelPowerSwitch = 0x10,
    kCmdPanelSwap = 0x12,
    kCmdAccelerateRate = 0x19,
    kCmdGyroRate = 0x1A,
    kCmdMagnetRate = 0x1B,
    kCmdParamReset = 0x1D,
    kCmdParamSave = 0x1F,
    kCmdPanelSet60fps = 0x20,
    kCmdPanelSet120fps = 0x21,
    kAckWheelKeyDoublePressed = 0x22,
    kCmdPanelGetFov = 0x23,
    KCmdAudioEnablePersistence = 0x25, //静音模式, 仅白羊1.0支持
    kCmdSideBySideChange = 0x30,
    kCmdTraceReport = 0x33, //设备ID
    kCmdAudioEnable = 0x34,
    kCmdPsensorEnable = 0x38,
    kCmdImuCalibration = 0x3C,
    kCmdGetGyroBias = 0x3E,
    kCmdSetGyroBias = 0x3F,
    kAckGlassesSleepNotify = 0x41,
    kAckGlassesWakeupNotify = 0x42,
    kCmdAudioMode = 0x49,
    kCmdVolumeSet = 0x50,
    kCmdVolumeUp = 0x51,
    kCmdVolumeDown = 0x52,
    kAckWheelKeyPressed = 0x54,
    kCmdLuminanceUp = 0x55,
    kCmdLuminanceDown = 0x56,
    kAckWheelKeyLongPressed = 0x57,
    kCmdWheelKeySideBySideDisable = 0x58,
    kAckImuData = 0x65,
    kCmdReboot2Bootloader = 0x66,
    kCmdCaluLum = 0x67,
    kCmdGetLT7211Version = 0x6A,
    kCmdPanelColorAdjust = 0x73,
    kAckCommand = 0xC8,
    kAckUsbCommandLog = 0xC9,
    kAckTraceReport = 0xCA,
    kCmdUsbCommandPARGESDump = 0xCB,
    kCmdTimeSynchronize = 0xE1, //设备间时钟同步
};


typedef struct {
    float tsb[3][3];
    float ta[3];
    float tg[3];
} XRImuCalibrateInfo;


/**
 * 新增成员请在结构体底部增加来确保向前兼容！！！
 * 并且修改ipc/XRRuntimeStateDesc.hpp涉及的定义！！！
 */
typedef struct {
    int32_t year;
    int32_t month;
    int32_t day;
} TIME;

typedef struct {
    float horizontalLeft;
    float horizontalRight;
    float verticalUp;
    float verticalDown;
} FOV;

typedef struct {
    uint8_t whiteBalanceWx;
    uint8_t whiteBalanceWy;
    uint8_t colorGainR;
    uint8_t colorGainG;
    uint8_t colorGainB;
    uint8_t colorEnhancementR;
    uint8_t colorEnhancementG;
    uint8_t colorEnhancementB;
    uint8_t contrast;
    uint8_t colorTemperature;
    uint8_t hue;
} XRPanelColorParms;

typedef union {
    XRPanelColorParms params;
    uint8_t data[12];
} PANELPARAMS;

typedef struct {
    char manufacturer[32];
    char hostManufacturer[32]; //Server所运行的设备manufacturer
    char glassesId[64];        //device id, SN码

    int32_t firmwareVersion;
    TIME time;

    bool wakeUp;   //true: wakeup, false: sleep
    bool dpStatus; //dp信号
    bool sideBySide;
    bool whisper;
    bool psensorState;
    bool psensorValid; //接近传感器使能状态, 0使能, 眼镜会自动休眠. 1关闭,眼镜always wakeup
    bool lsensorValid;
    bool gsensorValid;
    bool msensorValid;
    bool muteMode; //静音模式, 仅白羊1.0支持
    FOV fov;       //FOV, 数值采用角度制

    uint8_t volume; //current, [0, 12]
    uint8_t maxVolume;
    uint8_t luminance;    //current luminance level, [0, maxLuminance]
    uint8_t maxLuminance; //max luminance level
    uint8_t frameRate;    //fps

    PANELPARAMS panelParams; //屏幕参数
    bool supportPanelColorAdjust;

} XRDeviceState, XRDeviceInfo;

#ifdef __cplusplus
}
#endif