/*
 * Copyright (C) 2025 VITURE Inc. All rights reserved.
 */

#pragma once

// Unified platform-specific export macro
// Always exports (dllexport), never imports (no dllimport)
#if defined(_WIN32) || defined(__CYGWIN__)
#  define VITURE_API __declspec(dllexport)
#else
#  if defined(__GNUC__) || defined(__clang__)
#    define VITURE_API __attribute__((visibility("default")))
#  else
#    define VITURE_API
#  endif
#endif

#ifndef LOG_LEVEL_NONE
#  define LOG_LEVEL_NONE 0
#endif
#ifndef LOG_LEVEL_ERROR
#  define LOG_LEVEL_ERROR 1
#endif
#ifndef LOG_LEVEL_INFO
#  define LOG_LEVEL_INFO 2
#endif
#ifndef LOG_LEVEL_DEBUG
#  define LOG_LEVEL_DEBUG 3
#endif
