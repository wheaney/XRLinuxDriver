#ifndef FFALCONXR_TYPE_H
#define FFALCONXR_TYPE_H


#include "base/FXRMacro.h"
#include <limits.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct FXRRuntimeVersion {
    char commitTime[XR_CHARACTER_ARRAYS_MAX_LEN];   //build time
    char branch[XR_CHARACTER_ARRAYS_MAX_LEN]; //code branch
    char version[XR_CHARACTER_ARRAYS_MAX_LEN];
    char buildTime[XR_CHARACTER_ARRAYS_MAX_LEN];
} FXRRuntimeVersion;

enum FXRRuntimeStateType { kDevice = 0, kImu = 1 };


typedef enum FXRDeviceType {
    kProductUnknown = 0x00,
    kProductNextViewPro = 0x20,   //一代眼镜nextviewpro
    kProductAries = 0x21,         //二代眼镜白羊
    kProductAries1p5Seeya = 0x22, //白羊1.5 Seeya屏
    kProductAries1p5Sony = 0x23,  //白羊1.5 Sony屏
    kProductAries1p8 = 0x24,      //白羊1.8
    kProductTaurus = 0x30,        //金牛


    kProductMercury = 0x1000, //水星
} FXRDeviceType;

typedef enum FXREye {
    kEyeLeft = 0,
    kEyeRight,
    kNumEyes,
    kEyeMono = kEyeLeft,
    kEyeMultiview = kEyeLeft,
} FXREye;

typedef enum {
    PLANE_HORIZONTAL_UP,   /*like ceiling */
    PLANE_HORIZONTAL_DOWN, /*like floor or table*/
    PLANE_VERTICAL,        /*like wall*/
    PLANE_NON
} XRPlaneProperty;

enum FXRDistortionParamIndex {
    K1 = 0,
    K2 = 1,
    K3 = 2,
    P1 = 3,
    P2 = 4
};

typedef struct {
    bool deviceConnected;
    bool imuEnabled;
} XRRuntimeStateInfo;

typedef struct {
    uint64_t timestamp;
    float rotation[4]; //x,y,z,w
    float position[3];
} XRPoseInfo;

typedef struct XRINFO4PREDICTPOSE {
    float latestGyro[3];
    uint64_t latestSensorTimeStamp; //in nanoseconds
    XRPoseInfo latestPose;
} XRInfo4PredictPose;

typedef struct XRALGOSTATE {
    #ifdef __cplusplus
    XRALGOSTATE() {
        ffvinsStatus = -1;
        predictInfo = {.latestSensorTimeStamp = INT_MAX};
    }
    #endif
    int32_t ffvinsStatus;
    XRInfo4PredictPose predictInfo; //for pose prediction
} XRAlgoState;

typedef struct {
    struct {
        float x, y, z, w;
    } rotation;
    struct {
        float x, y, z;
    } position;
} HeadPose;

typedef struct {
    HeadPose pose;                  //!< Head pose
    int32_t poseStatus;             //!< Bit field (sxrTrackingMode) indicating pose status
    uint64_t poseTimeStampNs;       //!< Time stamp in which the head pose was generated (nanoseconds)
    uint64_t poseFetchTimeNs;       //!< Time stamp when this pose was retrieved
    uint64_t expectedDisplayTimeNs; //!< Expected time when this pose should be on screen (nanoseconds)
} HeadPoseState;

typedef struct {
    XRPoseInfo pose;
    float azimuth;
    float mag_field_strength;
    int mag_sensor_accuracy;
} XRNineAxisPoseInfo;

typedef struct {
    XRPlaneProperty property;
    XRPoseInfo pose;
    struct {
        float x, z;
    } local_range;
    float local_polygon[100];
    int32_t local_polygon_size;
} XRPlaneInfo;

typedef struct {
    XRPlaneInfo data[XR_PLANE_ARRAY_DEFAULT_SIZE];
    int32_t size;
} XRPlaneArray;

#ifdef __cplusplus
}
#endif
#endif //FFALCONXR_TYPE_H
