cmake_minimum_required(VERSION 3.16)
project(nrealAirDebugD3 C)

set(CMAKE_C_STANDARD 17)

add_library(
		nrealAirDebugD3
		src/debug.c
)

target_include_directories(nrealAirDebugD3
		BEFORE PUBLIC ${NRA_INCLUDE_DIR}
)

target_link_libraries(nrealAirDebugD3
		${NRA_LIBRARY}
)
