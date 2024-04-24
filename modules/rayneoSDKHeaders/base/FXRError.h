#ifndef FFALCONXR_ERROR_H
#define FFALCONXR_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stdint.h>

typedef enum {
    OK = 0,

    UNKNOWN_ERROR = INT32_MIN,

    FAILED = -EPERM,
    NO_MEMORY = -ENOMEM,
    UNSUPPORTED = -ENOSYS,
    INVALID_ARGS = -EINVAL,
    NOT_FOUND = -ENOENT,
    NO_DEVICE = -ENODEV,
    ALREADY_EXISTS = -EEXIST,
    DEAD_PIPE = -EPIPE,
    DATA_OVERFLOW = -EOVERFLOW,
    NO_DATA = -ENODATA,
    TRY_AGAIN = -EAGAIN,
    TIMED_OUT = -ETIMEDOUT,
    BAD_MSG = -EBADMSG,
    BAD_FD = -EBADFD,
} FXRResult;

#ifdef __cplusplus
}
#endif

#endif //FFALCONXR_ERROR_H
