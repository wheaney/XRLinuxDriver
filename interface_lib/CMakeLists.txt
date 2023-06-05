cmake_minimum_required(VERSION 3.16)
project(nrealAirLibrary C)

set(CMAKE_C_STANDARD 17)

add_subdirectory(hidapi)
find_package(json-c REQUIRED CONFIG)

add_subdirectory(Fusion/Fusion)
set(FUSION_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/interface_lib/Fusion)
set(FUSION_LIBRARY Fusion)

add_library(
		nrealAirLibrary
		src/crc32.c
		src/device3.c
		src/device4.c
)

target_compile_options(nrealAirLibrary PRIVATE -fPIC)

target_include_directories(nrealAirLibrary
		BEFORE PUBLIC include
)

target_include_directories(nrealAirLibrary
		BEFORE PRIVATE ${FUSION_INCLUDE_DIR}
)

target_link_libraries(nrealAirLibrary
		PRIVATE hidapi::hidapi json-c::json-c ${FUSION_LIBRARY}
)

set(NRA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
set(NRA_LIBRARY nrealAirLibrary PARENT_SCOPE)
