/*
 * Copyright (C) 2025 VITURE Inc. All rights reserved.
 */

#pragma once

#include "viture_macros_public.h"

// Version defined as macros so they can be used in preprocessor checks
#define VITURE_VERSION_MAJOR 2
#define VITURE_VERSION_MINOR 1
#define VITURE_VERSION_PATCH 1

// Helper macros to stringify numeric macros after expansion.
#define VITURE_STRINGIFY_IMPL(x) #x
#define VITURE_STRINGIFY(x) VITURE_STRINGIFY_IMPL(x)

// Compose the version string from the three numeric macros.
#define VITURE_VERSION_STRING              \
    VITURE_STRINGIFY(VITURE_VERSION_MAJOR) \
    "." VITURE_STRINGIFY(VITURE_VERSION_MINOR) "." VITURE_STRINGIFY(VITURE_VERSION_PATCH)

// C-style accessor exported with C linkage. Returns a pointer to a
// null-terminated static string owned by the library; callers must not free it.
extern "C" VITURE_API const char* GetVersionString();

namespace viture::version {
    // Expose version pieces as compile-time constants
    constexpr int kMajor = VITURE_VERSION_MAJOR;
    constexpr int kMinor = VITURE_VERSION_MINOR;
    constexpr int kPatch = VITURE_VERSION_PATCH;
} // namespace viture::version
