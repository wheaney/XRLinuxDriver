//
// Created by 曾滔 on 6/8/21.
//

#ifndef _GLASS_SDK_H_
#define _GLASS_SDK_H_

#define GLASS_SDK_NATIVE_VERSION "2.1.9"
#define ROKID_GLASS_VID (1234)

/*
 * Sensor Event Type.
 */
enum EVENT_TYPE{
    ACC_EVENT = 0x1<<0,
    GYRO_EVENT =  0x1<<1,
    FUSION_EVENT= 0x1<<2,
    KEY_EVENT= 0x1<<3,
    P_SENSOR_EVENT= 0x1<<4,
    TOUCH_EVENT = 0x1<<5,
    ROTATION_EVENT=0x1<<6,
    GAME_ROTATION_EVENT=0x1<<7,
    L_SENSOR_EVENT = 0x1<<8,
    MAGNET_EVENT = 0x1<<9,
    VSYNC_EVENT = 0x1<<10,
    RAW_EVENT=0x1<<31,
};

/*
 * Represent a vector in 3D space.
 */
typedef struct Axes{
    double x;
    double y;
    double z;
}_Axes;

/*
 * Sensor Data Type.
 */
typedef struct SensorData {
    uint64_t system_timestamp; // timestamp in nanoseconds when it's received (phone clock)
    uint64_t sensor_timestamp_ns; // sensor timestamp in nanoseconds (glass clock)
    uint32_t sequence; // reserved
    struct Axes data; // sensor data
}_SensorData;

/*
 * Represent fusion data, including accelerometer, gyroscope, and magnetometer data.
 * deprecated
 */
typedef struct FusionData{
    uint64_t system_timestamp;
    uint64_t sensor_timestamp_ns;
    uint32_t sequence;
    struct Axes acc;
    struct Axes gyro;
    struct Axes magnet;
}_FusionData;

/*
 * Represent rotation vector and game rotation vector
 */
typedef struct RotationData{
    uint64_t system_timestamp; // timestamp in nanoseconds when it's received (phone clock)
    uint64_t sensor_timestamp_ns; // sensor timestamp in nanoseconds (glass clock)
    uint32_t sequence; // reserved
    float Q[4]; // sensor data in quaternion
    int status; // reserved
}_RotationData;

/*
 * Represent key data
 * deprecated
 */
typedef struct KeyData{
    int keyCode;
    bool status;
}_KeyData;

/*
 * Represent event data
 * deprecated
 */
typedef struct EventData{
    uint32_t type;
    union{
        struct SensorData acc; // Accelerometer data
        struct SensorData gyro; // Gyroscope data
        struct SensorData magnet; // Magnetometer data
        struct FusionData imu; // FusionData, reserved
        struct RotationData rotation; // Rotation data
        uint64_t vsync_timestamp_ns; // VSync timestamp in nanoseconds
        int KeyData; // Key data, reserved
        bool pSensor; // Proximity sensor data, reserved
        uint8_t raw_data[64]; // Raw data in 64 bytes
    };
}_EventData;


/* Get usb context
 * Call after GlassEventInit or GlassControlInit
 *
 * \returns libusb_context*
 */
void* _Z21GlassSDKGetUsbContextv();

/* Init glass event and create instance
 *
 * \returns glass event instance
 */
void* _Z14GlassEventInitv(void);

/*
 * Open glass event instance with fd
 *
 * \param handle a glass event instance
 * \param fd the new file descriptor
 * \returns true on success, false on failure
 */
bool  _Z14GlassEventOpenPvi(void* handle, int fd);

/*
 * Open glass event instance with vid and pid
 *
 * \param handle a glass event instance
 * \param vid usb vendor id
 * \param pid usb product id
 * \returns true on success, false on failure
 */
bool  _Z14GlassEventOpenPvii(void* handle, int vid, int pid);

/*
 * Close glass event instance
 *
 * \param handle a glass event instance
 * \returns true on success, false on failure
 */
bool  _Z15GlassEventClosePv(void* handle);

/*
 * Release glass event instance
 *
 * \param handle a glass event instance
 * \returns true on success, false on failure
 */
bool  _Z17GlassEventReleasePv(void* handle);

/*event queue interface*/
/*
 * Register sensor event with required type
 *
 * \param handle a glass event instance
 * \param EVENT_TYPE the sensor event type
 * \returns a registered event
 */
void *GlassRegisterEvent(void* handle, enum EVENT_TYPE type);
/*
 * Register sensor event with required type and size
 *
 * \param handle a glass event instance
 * \param type the sensor event type
 * \param size the event number
 * \returns a registered event
 */
void *_Z26GlassRegisterEventWithSizePv10EVENT_TYPEi(void* handle, enum EVENT_TYPE type, int size);

/*
 * UnRegister sensor event
 *
 * \param handle a glass event instance
 * \param type the sensor event type
 * \returns true on success, false on failure
 */
bool _Z20GlassUnRegisterEventPvS_(void* handle, void* eventHandle);

/*
 * Get sensor event
 *
 * \param handle a glass event instance
 * \param eventHandle the sensor event handle
 * \param data the event data
 * \param timeout operation timed out
 * \returns true on success, false on failure
 */
bool  _Z14GlassWaitEventPvS_P9EventDatai(void* handle, void* eventHandle, struct EventData* data, int timeout);

/*
 * Get sensor event
 *
 * \param handle a glass event instance
 * \param eventHandle the sensor event handle
 * \param data the event data
 * \param timeout operation timed out
 * \returns 0  get event success
 *          1  timeout
 *          -1 GlassEvent closed
 *          -2 invalid param or GlassEventRelease
 */
int   _Z14GlassWaitEventPvS_P9EventDatai2(void* handle, void* ehandle, struct EventData* data, int timeout);

/*
 * enable or close fusion event
 *
 * \param handle a glass event instance
 * \param enable whether enable the fusion event or not
 */
void  _Z14AddFusionEventPvi(void* handle, int enable);


/* Init glass control and create instance
 *
 * \returns glass control instance
 */
void* _Z16GlassControlInitv   (void);

/*
 * Open glass control instance with fd
 *
 * \param handle a glass control instance
 * \param fd the new file descriptor
 * \returns true on success, false on failure
 */
bool  _Z16GlassControlOpenPvi   (void* handle, int fd);

/*
 * Open glass control instance with fd
 *
 * \param handle a glass control instance
 * \param vid usb vendor id
 * \param pid usb product id
 * \returns true on success, false on failure
 */
bool  _Z16GlassControlOpenPvii   (void* handle, int vid, int pid);

/*
 * Close glass control instance
 *
 * \param handle a glass control instance
 * \returns true on success, false on failure
 */
bool  _Z17GlassControlClosePv  (void* handle);

/*
 * Release glass control instance
 *
 * \param handle a glass control instance
 * \returns true on success, false on failure
 */
bool  _Z19GlassControlReleasePv(void* handle);

/*
 * Glass control interface
 */

/*
 * Set glass display mode
 *
 * \param handle a glass control instance
 * \param mode
 *          0   2D_3840*1080_60Hz
 *          1   3D_3840*1080_60Hz
 *          2   SBS //deprecated in some devices
 *          3   deprecated
 *          4   3D_3840*1200_90Hz
 *          5   3D_3840*1200_60Hz
 * \returns true on success, false on failure
 */
bool _Z19GlassSetDisplayModePvi(void*handle, int mode);

/*
 * Set glass volume
 *
 * \param handle a glass control instance
 * \param volume range from 0 to 100
 * \returns true on success, false on failure
 */
bool  SetVolume          (void*handle, int level);

/*
 * Set glass brightness
 *
 * \param handle a glass control instance
 * \param channel always 0
 * \param level range from 0 to 100
 * \returns true on success, false on failure
 */
bool  SetBrightness      (void*handle, int channel, int level);

/*
 * Get glass vsync status
 *
 * \param handle a glass control instance
 * \returns
 *      true    glass display is on
 *      false   glass display is off
 */
bool  GetVsyncStatus     (void*handle);

/*
 * Sync glass timestamp in us (glass clock) with target device(phone clock)
 * deprecated
 *
 * \param handle a glass control instance
 * \returns true on success, false on failure
 */
bool  GlassSyncTimeStamp (void*handle);

/*
 * Get glass brightness
 *
 * \param handle a glass control instance
 * \param channel always 0
 * \returns brightness, see \ref SetBrightness
 */
int   GetBrightness      (void*handle, int channel);

/*
 * Get glass volume
 *
 * \param handle a glass control instance
 * \returns volume, see \ref SetVolume
 */
int   GetVolume          (void*handle);

/*
 * Get glass display mode
 *
 * \param handle a glass control instance
 * \returns display mode, see \ref GetDisplayMode
 */
int   _Z14GetDisplayModePv     (void*handle);

/*
 * Get glass serial number
 *
 * \param handle a glass control instance
 * \returns serial number in char*
 */
char*  GetSerialNumber   (void*handle);

/*
 * Get glass seed
 *
 * \param handle a glass control instance
 * \returns seed in char*
 */
char*  GetSeed           (void*handle);

/*
 * Get glass firmware version
 *
 * \param handle a glass control instance
 * \returns firmware version in char*
 */
char*  GetFirmwareVersion(void*handle);

/*
 * Get glass hardware version
 *
 * \param handle a glass control instance
 * \returns hardware version in char*
 */
char*  GetHardwareVersion(void*handle);

/*
 * Get glass product name
 *
 * \param handle a glass control instance
 * \returns product name in char*
 */
char*  _Z14GetProductNamePv(void* handle);

/*
 * Get glass product id
 *
 * \param handle a glass control instance
 * \returns product id in char*
 */
int GetProductId(void* handle);

/*
 * Get glass vendor id
 *
 * \param handle a glass control instance
 * \returns vendor id in char*
 */
int GetVendorId(void* handle);

/*
 * Get timestamp in nano to sync glass time & phone time
 *
 * for example:
 *      long long delta_t_glass_to_phone = sysTimestamp - hmdTimestamp;
 *      long long timestamp = curTimestamp + detla_t_glass_to_phone;
 *
 * curTimestamp means actual timestamp in nano (imu or camera on phone clock)
 * timestamp means calculated timestamp in nano, reflect glass clock sensor data to phone clock
 *
 * \param handle a glass control instance
 * \param hmdTimestamp [out] glass timestamp in nano
 * \param sysTimestamp [out] phone timestamp in nano
 * \returns true on success, false on failure
 */
int GlassGetTimeNano(void *handle, long long *hmdTimestamp, long long *sysTimestamp);


// redefine the mangled function names here so we can use the API as intended
typedef bool (*GlassControlCloseFunc)(void* handle);
typedef bool (*GlassControlReleaseFunc)(void* handle);
typedef void* (*GlassControlInitFunc)(void);
typedef bool (*GlassControlOpenFunc)(void* handle, int vid, int pid);
typedef bool (*GlassSetDisplayModeFunc)(void* handle, int mode);
typedef void* (*GlassEventInitFunc)(void);
typedef void* (*GlassRegisterEventWithSizeFunc)(void* handle, enum EVENT_TYPE type, int size);
typedef bool (*GlassUnRegisterEventFunc)(void* handle, void* eventHandle);
typedef bool (*GlassWaitEventFunc)(void* handle, void* eventHandle, struct EventData* data, int timeout);
typedef void* (*GlassSDKGetUsbContextFunc)(void);
typedef void (*GlassAddFusionEventFunc)(void* handle, int enable);
typedef bool (*GlassEventOpenFunc)(void* handle, int vid, int pid);
typedef bool (*GlassEventCloseFunc)(void* handle);
typedef int (*GetDisplayModeFunc)(void* handle);
typedef char* (*GetProductNameFunc)(void* handle);

extern GlassControlCloseFunc GlassControlClose;
extern GlassControlReleaseFunc GlassControlRelease;
extern GlassControlInitFunc GlassControlInit;
extern GlassControlOpenFunc GlassControlOpen;
extern GlassSetDisplayModeFunc GlassSetDisplayMode;
extern GlassEventInitFunc GlassEventInit;
extern GlassRegisterEventWithSizeFunc GlassRegisterEventWithSize;
extern GlassUnRegisterEventFunc GlassUnRegisterEvent;
extern GlassWaitEventFunc GlassWaitEvent;
extern GlassSDKGetUsbContextFunc GlassSDKGetUsbContext;
extern GlassAddFusionEventFunc GlassAddFusionEvent;
extern GlassEventOpenFunc GlassEventOpen;
extern GlassEventCloseFunc GlassEventClose;
extern GetDisplayModeFunc GetDisplayMode;
extern GetProductNameFunc GetProductName;

// retrieved using readelf (e.g. `readelf -W -s lib/x86_64/libGlassSDK.so  | grep GlassControlClose`)
#define GlassControlClose ((GlassControlCloseFunc)_Z17GlassControlClosePv)
#define GlassControlRelease ((GlassControlReleaseFunc)_Z19GlassControlReleasePv)
#define GlassControlInit ((GlassControlInitFunc)_Z16GlassControlInitv)
#define GlassControlOpen ((GlassControlOpenFunc)_Z16GlassControlOpenPvii)
#define GlassSetDisplayMode ((GlassSetDisplayModeFunc)_Z19GlassSetDisplayModePvi)
#define GlassEventInit ((GlassEventInitFunc)_Z14GlassEventInitv)
#define GlassRegisterEventWithSize ((GlassRegisterEventWithSizeFunc)_Z26GlassRegisterEventWithSizePv10EVENT_TYPEi)
#define GlassUnRegisterEvent ((GlassUnRegisterEventFunc)_Z20GlassUnRegisterEventPvS_)
#define GlassWaitEvent ((GlassWaitEventFunc)_Z14GlassWaitEventPvS_P9EventDatai)
#define GlassSDKGetUsbContext ((GlassSDKGetUsbContextFunc)_Z21GlassSDKGetUsbContextv)
#define GlassAddFusionEvent ((GlassAddFusionEventFunc)_Z14AddFusionEventPvi)
#define GlassEventOpen ((GlassEventOpenFunc)_Z14GlassEventOpenPvii)
#define GlassEventClose ((GlassEventCloseFunc)_Z15GlassEventClosePv)
#define GetDisplayMode ((GetDisplayModeFunc)_Z14GetDisplayModePv)
#define GetProductName ((GetProductNameFunc)_Z14GetProductNamePv)

#endif //_GLASS_SDK_H_