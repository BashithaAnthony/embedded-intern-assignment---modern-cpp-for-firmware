add_library(task2_lib INTERFACE)
target_include_directories(task2_lib INTERFACE include)
target_link_libraries(task2_lib INTERFACE project_options)
target_link_libraries(task2_lib INTERFACE firmware_constraints)

add_executable(test_task2 tests/test_task2.cpp)
target_link_libraries(test_task2 PRIVATE task2_lib test_deps)
add_test(NAME task2 COMMAND test_task2)
