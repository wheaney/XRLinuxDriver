cmake_minimum_required(VERSION 3.16)
project(xrealAirDebugD3 C)

set(CMAKE_C_STANDARD 17)

add_executable(
		xrealAirDebugD3
		src/debug.c
)

target_include_directories(xrealAirDebugD3
		BEFORE PUBLIC ${XREAL_AIR_INCLUDE_DIR}
)

target_link_libraries(xrealAirDebugD3
		${XREAL_AIR_LIBRARY}
)
