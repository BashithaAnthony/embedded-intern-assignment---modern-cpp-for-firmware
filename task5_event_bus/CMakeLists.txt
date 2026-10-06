find_package(Threads REQUIRED)

add_library(task5_lib INTERFACE)
target_include_directories(task5_lib INTERFACE include)
target_link_libraries(task5_lib INTERFACE project_options)
target_link_libraries(task5_lib INTERFACE Threads::Threads)

add_executable(test_task5 tests/test_task5.cpp)
target_link_libraries(test_task5 PRIVATE task5_lib test_deps)
add_test(NAME task5 COMMAND test_task5)
