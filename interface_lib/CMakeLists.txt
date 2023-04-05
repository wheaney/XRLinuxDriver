cmake_minimum_required(VERSION 3.16)
project(nrealAirLibrary C)

set(CMAKE_C_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBUSB1 REQUIRED libusb-1.0)

add_library(
		nrealAirLibrary
		src/device3.c
		src/device4.c
)

target_include_directories(nrealAirLibrary
		BEFORE PUBLIC include
)

target_include_directories(nrealAirLibrary
		SYSTEM BEFORE PRIVATE ${FUSION_INCLUDE_DIR} ${LIBUSB1_INCLUDE_DIRS}
)

target_link_libraries(nrealAirLibrary
		${LIBUSB1_LIBRARIES} ${FUSION_LIBRARY} m
)

set(NRA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
set(NRA_LIBRARY nrealAirLibrary PARENT_SCOPE)
