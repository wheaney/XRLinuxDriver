cmake_minimum_required(VERSION 3.16)
project(xrealAirLinuxDriver C)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(UA_API_SECRET $ENV{UA_API_SECRET})

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set (py_cmd "${CMAKE_SOURCE_DIR}/bin/build_custom_banner_config.py")
set (custom_banner_header_file "${CMAKE_SOURCE_DIR}/include/custom_banner_config.h")
set (custom_banner_yaml_file "${CMAKE_SOURCE_DIR}/custom_banner_config.yml")

add_custom_target(run_python_script ALL
    COMMAND ${Python3_EXECUTABLE} ${py_cmd} ${custom_banner_yaml_file} ${custom_banner_header_file}
)

if(NOT "${UA_API_SECRET}" STREQUAL "")
    message(STATUS "Using UA_API_SECRET from environment")
    add_definitions(-DUA_API_SECRET="${UA_API_SECRET}")
endif()

find_package(Threads REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(LIBEVDEV REQUIRED)
find_package(libusb REQUIRED)
find_package(Curl REQUIRED)
find_package(OpenSSL REQUIRED)

set(VITURE_LIB_NAME viture_sdk)

execute_process(COMMAND git submodule update --init --recursive
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(modules/xrealInterfaceLibrary/interface_lib)

add_library (
	${VITURE_LIB_NAME}
        STATIC
	IMPORTED)

set_target_properties(
    ${VITURE_LIB_NAME} PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/libviture_one_sdk.a
)

add_executable(xrealAirLinuxDriver
		src/buffer.c
		src/config.c
		src/devices/viture.c
		src/devices/xreal.c
		src/driver.c
		src/files.c
		src/imu.c
		src/ipc.c
		src/multitap.c
		src/outputs.c
		src/plugins.c
		src/plugins/custom_banner.c
		src/plugins/metrics.c
		src/plugins/sideview.c
		src/plugins/virtual_display.c
		src/runtime_context.c
		src/state.c
		src/strings.c
)

target_include_directories(xrealAirLinuxDriver
		SYSTEM BEFORE PRIVATE
		${LIBEVDEV_INCLUDE_DIRS}
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/modules/xrealInterfaceLibrary/interface_lib/include
		${LIBUSB_INCLUDE_DIR}
		${CURL_INCLUDE_DIR}
		${OPENSSL_INCLUDE_DIRS}
)

target_link_libraries(xrealAirLinuxDriver
		PRIVATE ${LIBEVDEV_LIBRARIES}
		        nrealAirLibrary
		        Threads::Threads
		        m
		        ${VITURE_LIB_NAME}
		        ${LIBUSB_LIBRARY}
		        ${CURL_LIBRARY}
		        ${OPENSSL_LIBRARIES}
)
add_dependencies(xrealAirLinuxDriver run_python_script)