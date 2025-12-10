/**
 * @file
 * @brief Protocol constants and identifiers used by the Viture USB protocol.
 * @copyright 2025 VITURE Inc. All rights reserved.
 *
 * This header exposes constants for display modes, DOF settings, IMU modes
 * and callback identifiers. These constants are intended for use by
 * application code and protocol builders/parsers.
 */

#ifndef VITURE_PROTOCOL_H
#define VITURE_PROTOCOL_H

#include <cstdint>

namespace viture::protocol {
    /**
     * @brief Product market names
     */
    namespace MarketName {
        constexpr const char* ONE = "One";
        constexpr const char* PRO = "Pro";
        constexpr const char* LITE = "Lite";
        constexpr const char* LUMA = "Luma";
        constexpr const char* LUMA_PRO = "Luma Pro";
        constexpr const char* LUMA_ULTRA = "Luma Ultra";
        constexpr const char* LUMA_CYBER = "Luma Cyber";
        constexpr const char* BEAST = "Beast";
    }
    /**
     * @brief Result codes used by various protocol helpers and APIs.
     */
    namespace Result {
        constexpr int SUCCESS = 0; /**< Success code */
        constexpr int FAILURE = 1; /**< Failure code */
    }; // namespace Result

    /**
     * @brief Display mode identifiers for switching device display output
     * and refresh-rate configurations.
     */
    namespace DisplayMode {
        constexpr uint8_t MODE_1920_1080_60HZ = 0x31; /**< 1920x1080 @ 60Hz */
        constexpr uint8_t MODE_3840_1080_60HZ = 0x32; /**< 3840x1080 @ 60Hz */
        constexpr uint8_t MODE_1920_1080_90HZ = 0x33; /**< 1920x1080 @ 90Hz */
        constexpr uint8_t MODE_1920_1080_120HZ = 0x34; /**< 1920x1080 @ 120Hz */
        constexpr uint8_t MODE_3840_1080_90HZ = 0x35; /**< 3840x1080 @ 90Hz */
        constexpr uint8_t MODE_1920_1200_60HZ = 0x41; /**< 1920x1200 @ 60Hz */
        constexpr uint8_t MODE_3840_1200_60HZ = 0x42; /**< 3840x1200 @ 60Hz */
        constexpr uint8_t MODE_1920_1200_90HZ = 0x43; /**< 1920x1200 @ 90Hz */
        constexpr uint8_t MODE_1920_1200_120HZ = 0x44; /**< 1920x1200 @ 120Hz */
        constexpr uint8_t MODE_3840_1200_90HZ = 0x45; /**< 3840x1200 @ 90Hz */

        /**
         * Added for Beast
         */
        constexpr uint8_t MODE_ULTRAWIDE_60HZ = 0x51; /**< Ultrawide mode @ 60Hz */
        constexpr uint8_t MODE_SIDEMODE_60HZ = 0x61; /**< Side-by-side mode @ 60Hz */
    } // namespace DisplayMode

    /**
     * @brief Native device DOF modes (used for devices that support native 3DOF, e.g. Viture Beast).
     */
    namespace NativeDOF {
        constexpr uint8_t DOF_0 = 0x00; /**< No native DOF */
        constexpr uint8_t DOF_3 = 0x01; /**< Native 3DOF */
        constexpr uint8_t DOF_SMOOTH_FOLLOW = 0x02; /**< Smooth follow mode */
    } // namespace NativeDOF

    /**
     * @brief Duty cycle presets used by the display controller.
     */
    namespace DutyCycle {
        constexpr uint8_t H = 98; /**< High duty cycle */
        constexpr uint8_t M = 42; /**< Medium duty cycle */
        constexpr uint8_t L = 30; /**< Low duty cycle */
    } // namespace DutyCycle

    /**
     * @brief IMU configuration options.
     */
    namespace Imu {
        namespace Mode {
            constexpr uint8_t MODE_RAW = 0; /**< Report IMU raw data */
            constexpr uint8_t MODE_POSE = 1; /**< Report IMU pose data */
        } // namespace Mode

        namespace Frequency {
            constexpr uint8_t LOW = 0; /**< 60Hz */
            constexpr uint8_t MEDIUM_LOW = 1; /**< 90Hz */
            constexpr uint8_t MEDIUM = 2; /**< 120Hz */
            constexpr uint8_t MEDIUM_HIGH = 3; /**< 240Hz */
            constexpr uint8_t HIGH = 4; /**< 500Hz */
        } // namespace Frequency
    } // namespace Imu

    /**
     * @brief Callback identifiers used when reporting glass state changes.
     */
    namespace Callback {
        namespace ID {
            /**
             * @brief Brightness change
             *
             * | Device Model            | Value Range |
             * |-------------------------|-------------|
             * | Viture One & Viture Pro | [0, 6]      |
             * | Viture Luma Series      | [0, 8]      |
             * | Viture Beast            | [0, 8]      |
             */
            constexpr uint8_t BRIGHTNESS = 0;
            /**
             * @brief Volume change
             *
             * | Device Model       | Value Range |
             * |--------------------|-------------|
             * | Viture One         | [0, 7]      |
             * | Viture Pro         | [0, 8]      |
             * | Viture Luma Series | [0, 8]      |
             * | Viture Beast       | [0, 15]     |
             */
            constexpr uint8_t VOLUME = 1;
            /**
             * @brief Volume change
             *
             * See `DisplayMode` for possible values.
             */
            constexpr uint8_t DISPLAY_MODE = 2;
            /**
             * @brief Electronchromic film status change
             *
             * | Device Model            | Value Range |
             * |-------------------------|-------------|
             * | Viture One & Viture Pro | [0, 1]      |
             * | Viture Luma Series      | [0, 1]      |
             * | Viture Beast            | [0, 8]      |
             */
            constexpr uint8_t ELECTROCHROMIC_FILM = 3;
            /**
             * @brief Native dof type change
             *
             * See `NativeDOF` for possible values.
             */
            constexpr uint8_t NATIVE_DOF = 4;
        } // namespace ID
    } // namespace Callback
} // namespace viture::protocol

#endif // VITURE_PROTOCOL_H
