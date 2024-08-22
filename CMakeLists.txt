cmake_minimum_required(VERSION 3.16)
project(xrDriver VERSION 0.11.1.3 LANGUAGES C)

configure_file(include/version.h.in version.h)
include_directories(${PROJECT_BINARY_DIR})

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(PkgConfig REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set (py_cmd "${CMAKE_SOURCE_DIR}/bin/build_custom_banner_config.py")
set (custom_banner_header_file "${CMAKE_SOURCE_DIR}/include/custom_banner_config.h")
set (custom_banner_yaml_file "${CMAKE_SOURCE_DIR}/custom_banner_config.yml")

add_custom_target(run_python_script ALL
    COMMAND ${Python3_EXECUTABLE} ${py_cmd} ${custom_banner_yaml_file} ${custom_banner_header_file}
)

if(DEFINED ENV{UA_API_SECRET})
  set(UA_API_SECRET $ENV{UA_API_SECRET})
  message(STATUS "Using UA_API_SECRET from environment")
  add_definitions(-DUA_API_SECRET="${UA_API_SECRET}")
else()
  set(UA_API_SECRET "")
  message(STATUS "!!! UA_API_SECRET is NOT set")
endif()

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/license_public_key.pem DEVICE_LICENSE_PUBLIC_KEY_RAW)
string(REPLACE "\n" "\\n" DEVICE_LICENSE_PUBLIC_KEY_ESCAPED ${DEVICE_LICENSE_PUBLIC_KEY_RAW})
add_definitions(-DDEVICE_LICENSE_PUBLIC_KEY="${DEVICE_LICENSE_PUBLIC_KEY_ESCAPED}")

find_package(Threads REQUIRED)

pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(JSONC REQUIRED json-c)
pkg_check_modules(CURL REQUIRED libcurl)

execute_process(COMMAND git submodule update --init --recursive
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(modules/xrealInterfaceLibrary/interface_lib)

# Set the library directory based on architecture
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/${CMAKE_SYSTEM_PROCESSOR})
message(STATUS "LIB_DIR: ${LIB_DIR}")

# Find all .a files in the library directory
file(GLOB STATIC_LIBS ${LIB_DIR}/*.a)
message(STATUS "STATIC_LIBS: ${STATIC_LIBS}")

# Add all static libraries
foreach(LIB ${STATIC_LIBS})
    get_filename_component(LIB_NAME ${LIB} NAME_WE)
    string(REPLACE "lib" "" LIB_NAME ${LIB_NAME})
    
    add_library(${LIB_NAME} STATIC IMPORTED)
    set_target_properties(${LIB_NAME} PROPERTIES
        IMPORTED_LOCATION ${LIB}
    )
endforeach()

# Find all .so files in the library directory
file(GLOB SHARED_LIBS ${LIB_DIR}/*.so)
message(STATUS "SHARED_LIBS: ${SHARED_LIBS}")

# Add all shared libraries
foreach(LIB ${SHARED_LIBS})
    get_filename_component(LIB_NAME ${LIB} NAME_WE)
    string(REPLACE "lib" "" LIB_NAME ${LIB_NAME})
    
    add_library(${LIB_NAME} SHARED IMPORTED)
    set_target_properties(${LIB_NAME} PROPERTIES
        IMPORTED_LOCATION ${LIB}
        IMPORTED_SONAME ${LIB_NAME}.so
    )
endforeach()

# Define the mapping of binary names to source files
set(DEVICE_MAPPINGS
    "libviture_one_sdk.a:src/devices/viture.c"
    "libRayNeoXRMiniSDK.so:src/devices/rayneo.c"
    "libGlassSDK.so:src/devices/rokid.c"
)

set(SOURCES
    src/buffer.c
    src/config.c
    src/curl.c
    src/devices/xreal.c
    src/devices.c
    src/driver.c
    src/epoch.c
    src/features/breezy_desktop.c
    src/features/smooth_follow.c
    src/features/sbs.c
    src/files.c
    src/logging.c
    src/imu.c
    src/ipc.c
    src/multitap.c
    src/outputs.c
    src/plugins.c
    src/plugins/breezy_desktop.c
    src/plugins/custom_banner.c
    src/plugins/device_license.c
    src/plugins/metrics.c
    src/plugins/sideview.c
    src/plugins/smooth_follow.c
    src/plugins/virtual_display.c
    src/runtime_context.c
    src/state.c
    src/strings.c
    src/system.c
)

# Function to get the source file from a mapping
function(get_source_file BINARY_NAME RESULT_VAR)
    foreach(MAPPING ${DEVICE_MAPPINGS})
        string(REPLACE ":" ";" MAPPING_LIST ${MAPPING})
        list(GET MAPPING_LIST 0 MAPPED_BINARY)
        list(GET MAPPING_LIST 1 MAPPED_SOURCE)
        if(${BINARY_NAME} STREQUAL ${MAPPED_BINARY})
            set(${RESULT_VAR} ${MAPPED_SOURCE} PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${RESULT_VAR} "" PARENT_SCOPE)
endfunction()

# Get all targets in the current directory
get_property(ALL_TARGETS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY IMPORTED_TARGETS)

# Iterate over all targets
foreach(TARGET ${ALL_TARGETS})
    get_target_property(LIB_TYPE ${TARGET} TYPE)
    if(LIB_TYPE STREQUAL "STATIC_LIBRARY" OR LIB_TYPE STREQUAL "SHARED_LIBRARY")
        get_target_property(LIB_LOCATION ${TARGET} IMPORTED_LOCATION)
        if(LIB_LOCATION)
            get_filename_component(LIB_NAME ${LIB_LOCATION} NAME)
            get_source_file(${LIB_NAME} SOURCE_FILE)
            if(SOURCE_FILE)
                list(APPEND SOURCES ${SOURCE_FILE})
                string(TOUPPER ${TARGET} TARGET_UPPER)
                add_definitions(-D${TARGET_UPPER}_SUPPORTED)
                message(STATUS "${TARGET} support enabled, added ${SOURCE_FILE}")
            endif()
        endif()
    endif()
endforeach()

add_executable(xrDriver ${SOURCES})

target_include_directories(xrDriver
		SYSTEM BEFORE PRIVATE
		${LIBEVDEV_INCLUDE_DIRS}
		${JSONC_INCLUDE_DIRS}
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/modules/rayneoSDKHeaders
		${CMAKE_CURRENT_SOURCE_DIR}/modules/xrealInterfaceLibrary/interface_lib/include
		${LIBUSB_INCLUDE_DIRS}
		${LIBCURL_INCLUDE_DIRS}
		${OPENSSL_INCLUDE_DIRS}
)

set(ALL_IMPORTED_LIBS)
get_property(ALL_TARGETS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY IMPORTED_TARGETS)
foreach(TARGET ${ALL_TARGETS})
    get_target_property(LIB_TYPE ${TARGET} TYPE)
    if(LIB_TYPE STREQUAL "STATIC_LIBRARY" OR LIB_TYPE STREQUAL "SHARED_LIBRARY")
        list(APPEND ALL_IMPORTED_LIBS ${TARGET})
    endif()
endforeach()

target_link_libraries(xrDriver
    PRIVATE
        ${LIBEVDEV_LIBRARIES}
        ${JSONC_LIBRARIES}
        xrealAirLibrary
        Threads::Threads
        m
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${ALL_IMPORTED_LIBS}
        ${LIBUSB_LIBRARIES}
)
add_dependencies(xrDriver run_python_script)
