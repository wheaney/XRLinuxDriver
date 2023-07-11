cmake_minimum_required(VERSION 3.16)
project(xrealAirLinuxDriver C)

set(CMAKE_C_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(LIBEVDEV REQUIRED)

add_subdirectory(interface_lib)
add_subdirectory(examples)

add_executable(xrealAirLinuxDriver
		src/driver.c
)

target_include_directories(xrealAirLinuxDriver
		SYSTEM BEFORE PRIVATE ${FUSION_INCLUDE_DIR} ${LIBEVDEV_INCLUDE_DIRS}
)

target_include_directories(xrealAirLinuxDriver
		BEFORE PUBLIC ${XREAL_AIR_INCLUDE_DIR}
)

target_link_libraries(xrealAirLinuxDriver
		PRIVATE ${XREAL_AIR_LIBRARY} ${FUSION_LIBRARY} ${LIBEVDEV_LIBRARIES}
)