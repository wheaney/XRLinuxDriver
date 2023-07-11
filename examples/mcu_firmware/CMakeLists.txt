cmake_minimum_required(VERSION 3.16)
project(xrealAirUpgradeMCU C)

set(CMAKE_C_STANDARD 17)

add_executable(
        xrealAirUpgradeMCU
        src/upgrade.c
)

target_include_directories(xrealAirUpgradeMCU
		BEFORE PUBLIC ${XREAL_AIR_INCLUDE_DIR}
)

target_link_libraries(xrealAirUpgradeMCU
		${XREAL_AIR_LIBRARY}
)
