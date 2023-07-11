cmake_minimum_required(VERSION 3.16)
project(xrealAirDebugD4 C)

set(CMAKE_C_STANDARD 17)

add_executable(
		xrealAirDebugD4
		src/debug.c
)

target_include_directories(xrealAirDebugD4
		BEFORE PUBLIC ${XREAL_AIR_INCLUDE_DIR}
)

target_link_libraries(xrealAirDebugD4
		${XREAL_AIR_LIBRARY}
)
