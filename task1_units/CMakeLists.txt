add_library(task1_lib INTERFACE)
target_include_directories(task1_lib INTERFACE include)
target_link_libraries(task1_lib INTERFACE project_options)
target_link_libraries(task1_lib INTERFACE firmware_constraints)

add_executable(test_task1 tests/test_task1.cpp)
target_link_libraries(test_task1 PRIVATE task1_lib test_deps)
add_test(NAME task1 COMMAND test_task1)
