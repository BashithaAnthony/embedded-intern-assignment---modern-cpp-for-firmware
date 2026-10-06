add_library(task3_lib INTERFACE)
target_include_directories(task3_lib INTERFACE include)
target_link_libraries(task3_lib INTERFACE project_options)
target_link_libraries(task3_lib INTERFACE firmware_constraints)

add_executable(test_task3 tests/test_task3.cpp)
target_link_libraries(test_task3 PRIVATE task3_lib test_deps)
add_test(NAME task3 COMMAND test_task3)
