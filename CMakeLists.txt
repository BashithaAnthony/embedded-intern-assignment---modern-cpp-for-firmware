cmake_minimum_required(VERSION 3.20)
project(embedded_cpp_firmware LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)            # -std=c++17, not gnu++17
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/ProjectOptions.cmake)
include(cmake/TestDeps.cmake)

enable_testing()                         # must come before add_subdirectory

add_subdirectory(task1_units)
add_subdirectory(task2_registers)
add_subdirectory(task3_connection_fsm)
add_subdirectory(task4_sensors)
add_subdirectory(task5_event_bus)
