cmake_minimum_required(VERSION 3.10)
project(Task4Benchmark)

add_library(task4_lib INTERFACE)
target_include_directories(task4_lib INTERFACE include)
#target_link_libraries(task4_lib INTERFACE project_options)
#target_link_libraries(task4_lib INTERFACE firmware_constraints)

add_executable(test_task4 tests/test_task4.cpp)
target_link_libraries(test_task4 PRIVATE task4_lib test_deps)
add_test(NAME task4 COMMAND test_task4)

add_executable(bench_task4_virtual bench/bench_virtual.cpp bench/virtual_factory.cpp)
target_link_libraries(bench_task4_virtual PRIVATE task4_lib)

add_executable(bench_task4_crtp bench/bench_crtp.cpp)
target_link_libraries(bench_task4_crtp PRIVATE task4_lib)