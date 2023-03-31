cmake_minimum_required(VERSION 3.16)
project(nrealAirLinuxDriver)

set(CMAKE_C_STANDARD 17)

add_subdirectory(modules/Fusion/Fusion)

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBUSB1 REQUIRED libusb-1.0)

add_executable(nrealAirLinuxDriver
		src/device3.c
		src/device4.c
		src/driver.c
)

target_include_directories(nrealAirLinuxDriver
		SYSTEM BEFORE PRIVATE modules/Fusion ${LIBUSB1_INCLUDE_DIRS}
)

target_link_libraries(nrealAirLinuxDriver
		${LIBUSB1_LIBRARIES} Fusion m
)