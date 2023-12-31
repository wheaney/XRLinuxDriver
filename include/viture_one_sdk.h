#ifndef GLASSES_CONTROLLER_AIR_OTA_H
#define GLASSES_CONTROLLER_AIR_OTA_H

/**
 * A typical sequence might resemble the following:
 * @code
 *  1. init the usblib and provide the IMU callback:
 *     @ref init(CallbackIMU imuCallback, CallbackMCU mcuCallback)
 *  2. Control the IMU data reading thread by enabling or pausing it:
 *     @set_imu(bool onOff);
 *
 *  3. Release the USB resources: 
 *     @ref deinit()
 *
 * @endcode
 *
 */


#ifdef __cplusplus
extern "C" {
#endif

// The outcome of the command, such as ‘set_imu’,  when communicated to the glasses device determines the subsequent action or state. This result signifies the success or failure of the command execution, influencing functionalities like IMU data reporting or device settings.
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

// State Definitions
#define STATE_ON 1
#define STATE_OFF 0

#define ERR_WRITE_FAIL -1
#define ERR_RSP_ERROR -2
#define ERR_TIMEOUT -3

#define IMU_FREQUENCE_60 0
#define IMU_FREQUENCE_90 1
#define IMU_FREQUENCE_120 2
#define IMU_FREQUENCE_240 3

#define MCU_BRIGHTNESS_ADJUSTMENT_MSG 0x301
#define MCU_SBS_ADJUSTMENT_MSG 0x302
#define MCU_VOLUME_ADJUSTMENT_MSG 0x304

#define MCU_SBS_ADJUSTMENT_DISABLED 0x31
#define MCU_SBS_ADJUSTMENT_ENABLED 0x32

/**
 * Callback for IMU Data
 * data: the first 12 byte is the data of IMU, others are reserved   
 *	uint8_t eulerRoll[4];
 *	uint8_t eulerPitch[4];
 *	uint8_t eulerYaw[4];
 *
 * len: the length of data
 * ts: the timestamp
 */
typedef void (*CallbackIMU)(uint8_t *data, uint16_t len, uint32_t ts);

/*
 * Callback for event change from the Glasses, such as 3D/2D switch event
 * msgid
 */
typedef void (*CallbackMCU)(uint16_t msgid, uint8_t *data, uint16_t len, uint32_t ts);

/**
 * init the usblib and gets the glasses device
 *
 * imuCallback: the Callback to receive the IMU data
 * mcuCallback: the Callback to get the event from the glasses
 */
bool init(CallbackIMU imuCallback, CallbackMCU mcuCallback);

/**
 * Release the resources, paired with init
 */
void deinit();

/**
 * Opens or pauses the IMU event reporting
 * onOff  true the glass will report the imu data
 */
int set_imu(bool onOff);

/**
 * Returns the IMU reporting state
 * 1 for IMU data reporting is on, 0 for off, <0 for error
 */
int get_imu_state();

/**
 * Switches the resolution of the glasses
 * true: 3840*1080  false: 1920*1080
 */
int set_3d(bool onOff);

/**
 * Returns the resolution state of the glasses
 * 1 for 3840*1080, 0 for 1920*1080, <0 for error
 */
int get_3d_state();

/**
 * Control the IMU report frequence
 * Parameters:
 * value 0x00 for 60Hz, 0x01 for 90Hz, 0x02 for 120Hz, 0x03 for 240Hz
 * Return: 
 *  error code 
 */
int set_imu_fq(int value);

/**
 * Get the current IMU report frequency.
 * Return:
 *  Current report frequency
 */
int get_imu_fq();

#ifdef __cplusplus
}
#endif

#endif //GLASSES_CONTROLLER_AIR_OTA_H
