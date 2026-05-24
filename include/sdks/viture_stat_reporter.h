/**
 * @file
 * @brief Public C API for optional SDK usage statistics reporting.
 * @copyright 2026 VITURE Inc. All rights reserved.
 *
 * Allows developers integrating libglasses to report SDK usage events (e.g.
 * glasses bind) to Viture's server for gain-sharing. Reporting is opt-in:
 * the developer must call `xr_stat_reporter_init` once with their AK/SK,
 * then call `xr_stat_reporter_glasses_bind` at the moment of their choice.
 *
 * Reporting calls are synchronous: they block on the calling thread until the
 * HTTP request completes (or times out) and return the server's response.
 */

#ifndef VITURE_STAT_REPORTER_H
#define VITURE_STAT_REPORTER_H

#include "viture_glasses_provider.h"
#include "viture_macros_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the stat reporter with developer credentials.
 *
 * Must be called after `xr_device_provider_initialize()` and before any
 * `xr_stat_reporter_*` event-report call. Credentials are stored in memory
 * only and are never transmitted as-is; the SK is used locally to sign
 * each request with HMAC-SHA1.
 *
 * @param handle   Handle returned by `xr_device_provider_create`.
 * @param ak       Access Key assigned by Viture (null-terminated).
 * @param sk       Secret Key assigned by Viture (null-terminated).
 * @param app_name Developer's app identifier (null-terminated).
 * @return VITURE_GLASSES_SUCCESS on success, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM null handle or empty credential
 */
VITURE_API int xr_stat_reporter_init(XRDeviceProviderHandle handle,
                                     const char* ak,
                                     const char* sk,
                                     const char* app_name);

/**
 * @brief Report a glasses bind event to Viture's server (synchronous).
 *
 * Sends the HTTP request on the calling thread and blocks until it completes
 * or times out. The return value reports network success/failure; pass a
 * non-null `http_status` to receive the exact HTTP status code on completion.
 *
 * @param handle      Handle returned by `xr_device_provider_create`.
 * @param http_status Optional out parameter receiving the HTTP status code
 *                    (e.g. 200) when the request reached the server, or 0 if
 *                    it never did. May be null.
 * @return VITURE_GLASSES_SUCCESS if the server returned a 2xx status,
 *         VITURE_GLASSES_ERROR_UNKNOWN on any other network outcome, or:
 *         - VITURE_GLASSES_ERROR_INVALID_PARAM null handle
 *         - VITURE_GLASSES_ERROR_INVALID_STATE reporter not initialized
 *         - VITURE_GLASSES_ERROR_NO_DATA       could not collect bind params
 */
VITURE_API int xr_stat_reporter_glasses_bind(XRDeviceProviderHandle handle,
                                             int* http_status);

#ifdef __cplusplus
}
#endif

#endif // VITURE_STAT_REPORTER_H
