cmake_minimum_required(VERSION 3.16)
project(nrealAirDebugD4 C)

set(CMAKE_C_STANDARD 17)

add_library(
		nrealAirDebugD4
		src/debug.c
)

target_include_directories(nrealAirDebugD4
		BEFORE PUBLIC ${NRA_INCLUDE_DIR}
)

target_link_libraries(nrealAirDebugD4
		${NRA_LIBRARY}
)
