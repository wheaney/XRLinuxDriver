/*
 * Copyright (C) 2026 Viture Inc. All rights reserved.
 */

#pragma once

/**
 * @file
 * @brief Standard result and error codes for all libglasses API calls.
 *
 * All API functions return VITURE_GLASSES_SUCCESS (0) on success,
 * or a negative error code on failure.
 */

/** Operation completed successfully. */
#define VITURE_GLASSES_SUCCESS         0

/** Null handle, null output pointer, or argument value out of range. */
#define VITURE_GLASSES_ERROR_INVALID_PARAM   -1

/** USB connection is not available or not established. */
#define VITURE_GLASSES_ERROR_USB_UNAVAILABLE -2

/** USB read/write operation failed. */
#define VITURE_GLASSES_ERROR_USB_EXEC        -3

/** Feature not supported by this device model. */
#define VITURE_GLASSES_ERROR_NOT_SUPPORTED   -4

/** No valid response data received from the device (timeout or empty). */
#define VITURE_GLASSES_ERROR_NO_DATA         -5

/** Response data format or length mismatch. */
#define VITURE_GLASSES_ERROR_DATA_PARSE      -6

/** Device rejected the command or value at the firmware level. */
#define VITURE_GLASSES_ERROR_DEVICE_REJECTED -7

/** Calibration initialization failed (xr_device_provider_initialize only). */
#define VITURE_GLASSES_ERROR_CALIB_INIT      -8

/** Serial number retrieval failed (xr_device_provider_initialize only). */
#define VITURE_GLASSES_ERROR_SERIAL_FETCH    -9

/** Operation not valid in the current state (e.g. already streaming, not initialized). */
#define VITURE_GLASSES_ERROR_INVALID_STATE  -10

/** Unclassified error. */
#define VITURE_GLASSES_ERROR_UNKNOWN        -99
