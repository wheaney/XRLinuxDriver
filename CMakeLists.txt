cmake_minimum_required(VERSION 3.16)
project(xrealAirLinuxDriver C)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(LIBEVDEV REQUIRED)

execute_process(COMMAND git submodule update --init --recursive
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(modules/xrealInterfaceLibrary/interface_lib)

add_executable(xrealAirLinuxDriver
		src/buffer.c
		src/config.c
		src/driver.c
		src/files.c
		src/ipc.c
		src/multitap.c
		src/outputs.c
		src/strings.c
		src/xreal.c
)

target_include_directories(xrealAirLinuxDriver
		SYSTEM BEFORE PRIVATE
		${LIBEVDEV_INCLUDE_DIRS}
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/modules/xrealInterfaceLibrary/interface_lib/include
)

target_link_libraries(xrealAirLinuxDriver
		PRIVATE ${LIBEVDEV_LIBRARIES} nrealAirLibrary Threads::Threads m
)