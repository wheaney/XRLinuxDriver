#ifndef FFALCONXR_PACKET_H
#define FFALCONXR_PACKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

typedef struct {
    uint8_t magic;
    uint8_t type;
    uint8_t value;
    uint8_t data[52];
    uint8_t reserved[9];
} XrHidCommand;

typedef struct {
    uint8_t magic;
    uint8_t type;
    uint16_t length;
} XrHidMsgHeader;

typedef struct {
    float acc[3];
    float gyro[3];
    float temperature;
    float magnet[2];
    uint32_t tick;
    float psensor;
    float lsensor;
    float magnet_2;     //magnet第三个字节
} XrHidSensorData;

typedef struct {
    XrHidMsgHeader head;
    XrHidSensorData sensor;
    union {
        struct {
            uint32_t count;
            uint8_t reserved[2];
            uint8_t checksum;
            uint8_t flag;
        } normal;
        struct {
            uint32_t left;
            uint32_t right;
        } vsync;
    } end;
} XrHidSensorEvent;

typedef struct {
    XrHidMsgHeader head;
    uint32_t tick;
    uint8_t value;
    uint8_t cpuid[12]; //unique ID of MCU Device
    uint8_t board_id;  //0x21 aries; //0x22 aries1p5_v1; //0x23 aries1p5 v2s
    uint8_t sensor_on;
    uint8_t support_fov; //1 for support, others for not support
    uint8_t date[12];    //example: “May 05 2020”
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t glasses_fps; //120=120fps, others=60fps
    uint8_t luminance;
    uint8_t volume;       // 0 ~ 12，0最小，12最大
    uint8_t side_by_side; //1: sidebyside; 0: mirror
    uint8_t psesnor_enable;
    uint8_t audio_mode; //0 for normal mode; 1 for quiet mode
    uint8_t dp_status;  //0 for dp not ready; 1 for dp ready
    uint8_t status3;
    uint8_t psensor_valid;
    uint8_t lsensor_valid;
    uint8_t gyro_valid;
    uint8_t magnet_valid;
    float reserve1; //smart PA left
    float reserve2; //smart PA right
    uint8_t max_luminance;
    uint8_t max_volume;
    uint8_t support_panel_color_adjust;
    uint8_t flag;
} XrHidDeviceInfo;

typedef struct {
    XrHidMsgHeader head;
    uint32_t tick;
    uint8_t value;
    uint8_t data[52];
    uint8_t reserved;
    uint8_t checksum;
    uint8_t flag;
} XrHidResponse;

typedef struct {
    XrHidMsgHeader head;
    uint32_t tick; //此次消息由设备发往host时的时间戳
    uint8_t value;
    uint8_t index; //时间同步response会连续发送10条
    uint8_t reserve[2];
    uint32_t receiveTick; //收到时间同步请求时的时间戳
    uint8_t reserve2[48];
} XrHidTimeSyncResponse;

typedef struct {
    uint32_t event;
    uint32_t length;
    uint64_t timestamp;
    uint8_t data[0];
} XrStateEvent;

typedef struct {
    uint64_t host_time; //host time
    /**
     * 分体式 & 使用USB协议的设备, Vsync信号随IMU数据一起捎带给出
     * 设计上携带了上一次Vsync的时间戳以及对应的SensorTime
     * 由这两个数据估计得到下一次vsync
     */
    uint64_t last_vsync;  //sensor time
    uint64_t sensor_time; //sensor time
} XrVsyncEvent;

typedef union {
    uint8_t data[64];
    XrHidResponse response;
    XrHidSensorEvent sensor_data;
    XrHidDeviceInfo device_info;
    XrStateEvent state_event;
    XrVsyncEvent vsync_event;
} XrDataPacket;
static_assert(sizeof(XrDataPacket) == 64, "XrDataPacket size must be 64");

typedef struct {
    XrHidMsgHeader head;
    uint32_t tick;
    uint8_t type;
    uint8_t left_low;
    uint8_t left_high;
    uint8_t right_low;
    uint8_t right_high;
    uint8_t up_low;
    uint8_t up_high;
    uint8_t down_low;
    uint8_t down_high;
} XrHidDeviceFov;

typedef struct {
    uint8_t index;
    int64_t timestamp;
    int64_t exposure_time;
} XrFrameEvent;

#ifdef __cplusplus
}
#endif

#endif //FFALCONXR_PACKET_H
