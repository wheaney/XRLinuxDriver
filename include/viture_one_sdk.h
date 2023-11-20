#ifndef GLASSES_CONTROLLER_AIR_OTA_H
#define GLASSES_CONTROLLER_AIR_OTA_H

/**
 * A typical loop might be something like this:
 * @code
 *  1. init the usblib and pass the IMU callback
 *     @ref init(CallbackIMU imuCallback, CallbackMCU mcuCallback)
 *  2. open/pause the reading thread of IMU
 *     @set_imu(bool onOff);
 *
 *  3. release the usb 
 *     @ref deinit()
 *
 * @endcode
 *
 */

// The result of Command into the glass like set_imu
#define    ERR_SUCCESS  0
#define    ERR_FAILURE  1
#define    ERR_INVALID_ARGUMENT  2
#define    ERR_NOT_ENOUGH_MEMORY  3
#define    ERR_UNSUPPORTED_CMD  4
#define    ERR_CRC_MISMATCH  5
#define    ERR_VER_MISMATCH  6  
#define    ERR_MSG_ID_MISMATCH  7
#define    ERR_MSG_STX_MISMATCH  8
#define    ERR_CODE_NOT_WRITTEN  9

// The result of state
#define STATE_OPEN 1
#define STATE_OFF 0

/**
 * Callback for IMU Data
 * data the first 12 byte is the data of IMU, others are reserved   
 *	uint8_t eulerRoll[4];
 *	uint8_t eulerPitch[4];
 *	uint8_t eulerYaw[4];
 *
 * len the length of data
 * ts the timestamp
 */
typedef void (*CallbackIMU)(uint8_t *data, uint16_t len, uint32_t ts);

/*
 * Callback for event change from the Glass, like 3D/2D change event
 * msgid
 */
typedef void (*CallbackMCU)(uint16_t msgid, uint8_t *data, uint16_t len, uint32_t ts);

/**
 * init the usblib and get the device of Glass
 *
 * imuCallback the Callback to receive the imu data
 * mcuCallback the Callback to get the event from the Glass
 */
bool init(CallbackIMU imuCallback, CallbackMCU mcuCallback);

/**
 * release the resources, paired with init
 */
void deinit();

/**
 * open and pause the imu event
 * onOff  true the glass will report the imu data
 */
int set_imu(bool onOff);

/**
 * return the imu report state
 * 1 means the glass will report the imu data, 0 means not
 */
int get_imu_state();

/**
 * swith the resolution of the glass
 * true : 3840*1080  false : 1920*1080
 */
int set_3d(bool onOff);

/**
 * return the resolution state
 * 1 means 3840*1080, 0 means 1920*1080
 */
int get_3d_state();

#endif //GLASSES_CONTROLLER_AIR_OTA_H
