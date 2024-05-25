cmake_minimum_required(VERSION 3.16)
project(xrealAirLinuxDriver C)

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

set(VITURE_LIB_NAME viture_sdk)

execute_process(COMMAND git submodule update --init --recursive
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(modules/xrealInterfaceLibrary/interface_lib)
add_subdirectory(modules/curl)

add_library (
	${VITURE_LIB_NAME}
        STATIC
	IMPORTED)

set_target_properties(
    ${VITURE_LIB_NAME} PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/libviture_one_sdk.a
)

set(SOURCES
    src/buffer.c
    src/config.c
    src/curl.c
    src/devices/viture.c
    src/devices/xreal.c
    src/driver.c
    src/features/breezy_desktop.c
    src/features/smooth_follow.c
    src/features/sbs.c
    src/files.c
    src/imu.c
    src/ipc.c
    src/multitap.c
    src/outputs.c
    src/plugins.c
    src/plugins/custom_banner.c
    src/plugins/device_license.c
    src/plugins/metrics.c
    src/plugins/smooth_follow.c
    src/runtime_context.c
    src/state.c
    src/strings.c
    src/system.c
)

if(DEFINED ENV{BREEZY_DESKTOP})
  message(STATUS "Breezy Desktop build")
  add_definitions(-DBREEZY_DESKTOP)
  list(APPEND SOURCES
    src/plugins/breezy_desktop.c
  )
else()
  message(STATUS "Standard build")
  list(APPEND SOURCES
    src/plugins/sideview.c
    src/plugins/virtual_display.c
  )
endif()

add_executable(xrealAirLinuxDriver ${SOURCES})

target_include_directories(xrealAirLinuxDriver
		SYSTEM BEFORE PRIVATE
		${LIBEVDEV_INCLUDE_DIRS}
		${JSONC_INCLUDE_DIRS}
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/modules/xrealInterfaceLibrary/interface_lib/include
		${LIBUSB_INCLUDE_DIRS}
		${LIBCURL_INCLUDE_DIRS}
		${OPENSSL_INCLUDE_DIRS}
)

target_link_libraries(xrealAirLinuxDriver
		PRIVATE ${LIBEVDEV_LIBRARIES}
		        ${JSONC_LIBRARIES}
		        nrealAirLibrary
		        Threads::Threads
		        m
		        libcurl
		        ${OPENSSL_LIBRARIES}
		        ${VITURE_LIB_NAME}
		        ${LIBUSB_LIBRARIES}
)
add_dependencies(xrealAirLinuxDriver run_python_script)