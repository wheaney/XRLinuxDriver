cmake_minimum_required(VERSION 3.16)
project(nrealAirUpgradeMCU C)

set(CMAKE_C_STANDARD 17)

add_executable(
        nrealAirUpgradeMCU
        src/upgrade.c
)

target_include_directories(nrealAirUpgradeMCU
		BEFORE PUBLIC ${NRA_INCLUDE_DIR}
)

target_link_libraries(nrealAirUpgradeMCU
		${NRA_LIBRARY}
)
