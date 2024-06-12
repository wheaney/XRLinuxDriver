#ifndef FFALCONXR_API_H
#define FFALCONXR_API_H

#include "base/FXRMacro.h"
#include "base/FXRType.h"
#include "device/usb/XRDeviceState.h"
#include "ipc/FXRProtocol.h"


extern "C" {


/**
 * 初始化
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL InitInstance();

/**
 * IMU传感器的数据回调
 * @param acc       加速度计数据
 * @param gyro      陀螺仪数据
 * @param mag       磁力计数据
 * @param timeInNs  采集数据时间
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL RegisterIMUEventCallback(IMUEventCallback callback);
RAYNEO_API_EXPORT void RAYNEO_API_CALL UnregisterIMUEventCallback(IMUEventCallback callback);

/**
 * Vsync事件回调
 * @param leftEyeInMs   左眼屏幕的Vsync时间
 * @param rightEyeInMs  右眼屏幕的Vsync时间
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL RegisterVsyncEventCallback(VsyncEventCallback callback);
RAYNEO_API_EXPORT void RAYNEO_API_CALL UnregisterVsyncEventCallback(VsyncEventCallback callback);

/**
 * 运行状态事件回调
 * @param state 状态编码
 * @param data  状态信息
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL RegisterStateEventCallback(StateEventCallback callback);
RAYNEO_API_EXPORT void RAYNEO_API_CALL UnregisterStateEventCallback(StateEventCallback callback);

/**
 * 相机帧参数回调 (需先调用AcquireHardwareBuffer)
 * @param buffer       图像缓冲区
 * @param timestamp    时间戳
 * @param exposureTime 曝光时间
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL RegisterFrameEventCallback(FrameEventCallback callback);
RAYNEO_API_EXPORT void RAYNEO_API_CALL UnregisterFrameEventCallback(FrameEventCallback callback);

/**
 * 相机帧数据回调
 * @param data         图像原始数据
 * @param w            宽
 * @param h            高
 * @param timestamp    时间戳
 * @param exposureTime 曝光时间
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL RegisterFrameDataCallback(FrameDataCallback callback);
RAYNEO_API_EXPORT void RAYNEO_API_CALL UnregisterFrameDataCallback(FrameDataCallback callback);

/**
 * 启动XR模式，进入Runtime的生命周期
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL StartXR();

/**
 * 停止XR模式，退出Runtime的生命周期
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL StopXR();

/**
 * 切换到2D镜像模式
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL SwitchTo2D();

/**
 * 切换到3D的双目模式
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL SwitchTo3D();

/**
 * 开启眼镜上报IMU数据
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL OpenIMU();

/**
 * 关闭眼镜上报IMU数据
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL CloseIMU();

/**
 * 设置声音模式
 * @param mode 0 for 普通模式, 1 for 轻语模式
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL SetAudioMode(uint8_t mode);

/**
 * 设置音量
 * @param val volume index (0~12)
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL SetAudioVolume(uint8_t val);

/**
 * Disable hardware SBS switch
 * @param seconds 时长
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL DisableWheelKeySideBySide(uint8_t seconds);

/**
 * 保存设置
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL SaveSettings();

/**
 * 请求设备信息
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL AcquireDeviceInfo();

/**
 * 读取设备信息
 * @return 设备信息
 */
RAYNEO_API_EXPORT XRDeviceInfo RAYNEO_API_CALL FetchDeviceInfo();

/**
 * Panel Display on
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL PanelPowerOn();

/**
 * Panel Display off
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL PanelPowerOff();

/**
 * Swap Two Panels Display Content
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL PanelPowerSwap();

/**
 * set panel preset luminance
 * @param val
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL PanelLunaSet(uint8_t val);

/**
 * save luminance value to ROM
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL PanelLunaSave();

/**
 * set frame rate of device
 * only support 120fps and 60fps
 * @param val 120 means 120fps, other value means 60fps
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL PanelFrameRateSet(uint8_t val);

/**
 * Reset all saved parameters
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL ResetSettings();

/**
 * Change SIDE_BY_SIDE on/off
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL SwitchSideBySide();

/**
 * 音频使能
 * @param channel  0 for left, 1 for right
 * @param state  0 for disable, 1 for enable
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL EnableAudio(uint8_t channel, uint8_t state);

/**
 * P-Sensor detect enable/disable
 * @param state  0 for enable P-Sensor, 1 for Always wakeup
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL EnablePSensorDetect(uint8_t state);

/**
 * Reboot the device to boot loader
 * @param seconds
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL RebootAndBootloader();

/**
 * set mute mode, 仅白羊1.0支持
 * @param mute
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL EnableAudioPersistence(uint8_t mute);

/**
 * 屏幕颜色参数设置
 * @param op 具体操作
 * @param value 值
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL PanelColorParamsAdjust(uint8_t op, uint8_t value);

//--------------------- Tracking -----------------------------

/**
 * Get/Predict pose of device
 * @param offsetInNs 0 means current pose, other means predict
 * @param rotation Quart(x, y, z, w)
 * @param position Vector3(x, y, z)
 */
RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL GetHeadTrackerPose(uint64_t offsetInNs, float* rotation, float* position);

/**
 * Get the current pose of mobile device
 * @param rotation Quart(x, y, z, w)
 * @param position Vector3(x, y, z)
 */
RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL GetMobileTrackerPose(float* rotation, float* position);

/**
 * Get current status of algo
 * @return status
 */
RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL GetHeadTrackerStatus();

RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL GetNineAxisOrientation(float* orientation);

RAYNEO_API_EXPORT float RAYNEO_API_CALL GetNineAxisAzimuth();

RAYNEO_API_EXPORT float RAYNEO_API_CALL GetMagnetometerFieldStrength();

RAYNEO_API_EXPORT int RAYNEO_API_CALL GetMagnetometerSensorAccuracy();

/**
 * Recenter the internal head tracker pose
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL RecenterHeadTracker();

RAYNEO_API_EXPORT void RAYNEO_API_CALL EnableSlamHeadTracker();

RAYNEO_API_EXPORT void RAYNEO_API_CALL DisableSlamHeadTracker();

RAYNEO_API_EXPORT void RAYNEO_API_CALL EnablePlaneDetection();

RAYNEO_API_EXPORT void RAYNEO_API_CALL DisablePlaneDetection();

/**
 * Get Plane Info
 * @param XRPlaneArray
 * @return
 */
RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL GetPlaneInfo(XRPlaneArray* planeInfo);

// TODO:后续需要传入参数配置Camera输出流
RAYNEO_API_EXPORT void RAYNEO_API_CALL OpenCamera();

// 临时接口, 指定最大画幅比16:9分辨率打开相机
RAYNEO_API_EXPORT void RAYNEO_API_CALL OpenMaxResCamera();

RAYNEO_API_EXPORT void RAYNEO_API_CALL CloseCamera();

/**
 * Acquire hardware buffers
 *
 * @param buffers           is a pointer to an array of AHardwareBuffer* used to store the acquired
 *                          hardware buffers. , but can be NULL if capacityInput is 0.
 * @param capacityInput     is the capacity of the buffers array, or 0 to indicate a request to
 *                          retrieve the required capacity.
 * @param countOutput       pointer to the count of buffers written, or a pointer to the required
 *                          capacity in the case that capacityInput is 0.
 * @return Error code or success flag.
 */
RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL AcquireHardwareBuffer(AHardwareBuffer** buffers, uint8_t capacityInput, uint8_t* countOutput);

/**
 * Get the latest AHardwareBuffer
 *
 * @param buffer         the latest AHardwareBuffer
 * @param timestamp      the timestamp of the latest frame
 * @param exposureTime   the exposure time of the latest frame
 * @return Error code or success flag.
 */
RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL GetLatestBuffer(AHardwareBuffer** buffer, int64_t* timestamp, int64_t* exposureTime);

/**
 * Get the latest frame
 *
 * @param data           the latest frame data
 * @param width          the width of the latest frame
 * @param height         the height of the latest frame
 * @param timestamp      the timestamp of the latest frame
 * @param exposureTime   the exposure time of the latest frame
 * @return Error code or success flag.
 */
RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL
GetLatestFrame(uint8_t** data, uint32_t* width, uint32_t* height, int64_t* timestamp, int64_t* exposureTime);

/**
 * Send command to runtime
 * @param unit runtime unit
 * @param command command detail
 * @param param
 * @param len
 * @return
 */
RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL SendCommand(FXRControlUnit unit,
                                                      FXRControlCommand command,
                                                      void* param = nullptr,
                                                      uint32_t len = 0);

/**
 * set property
 * @param item
 * @param value
 * @param len
 * @return
 */
RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL SetProp(const char* item, void* value, const int32_t len);

/**
 * get set property
 * @param item
 * @param value
 * @param len
 * @return
 */
RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL GetProp(const char* item, void* value, int32_t& len);


//--------------------- Query -----------------------------

/**
 * Query the specific type's code of Runtime state
 */
RAYNEO_API_EXPORT int32_t RAYNEO_API_CALL QueryRuntimeState(FXRRuntimeStateType type);


//--------------------- Configuration ----------------------
/**
 * read configuration from external
 * @param reader callback
 */
RAYNEO_API_EXPORT void RAYNEO_API_CALL RegisterXRConfigurationExternalReader(XRConfigExternalReader reader);
}


#endif //FFALCONXR_API_H
