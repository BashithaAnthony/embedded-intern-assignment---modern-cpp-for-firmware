add_library(task4_lib INTERFACE)
target_include_directories(task4_lib INTERFACE include)
target_link_libraries(task4_lib INTERFACE project_options)
target_link_libraries(task4_lib INTERFACE firmware_constraints)

add_executable(test_task4 tests/test_task4.cpp)
target_link_libraries(test_task4 PRIVATE task4_lib test_deps)
add_test(NAME task4 COMMAND test_task4)
