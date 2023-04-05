cmake_minimum_required(VERSION 3.16)
project(nrealAirLinuxDriver)

set(CMAKE_C_STANDARD 17)

add_subdirectory(modules/Fusion/Fusion)
set(FUSION_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/modules/Fusion)
set(FUSION_LIBRARY Fusion)

add_subdirectory(interface_lib)
add_subdirectory(examples)

add_executable(nrealAirLinuxDriver
		src/driver.c
)

target_include_directories(nrealAirLinuxDriver
		SYSTEM BEFORE PRIVATE ${FUSION_INCLUDE_DIR}
)

target_include_directories(nrealAirLinuxDriver
		BEFORE PUBLIC ${NRA_INCLUDE_DIR}
)

target_link_libraries(nrealAirLinuxDriver
		${NRA_LIBRARY} ${FUSION_LIBRARY}
)