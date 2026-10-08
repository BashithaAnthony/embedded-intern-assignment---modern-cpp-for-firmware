add_library(task4_lib INTERFACE)
target_include_directories(task4_lib INTERFACE include)
target_link_libraries(task4_lib INTERFACE project_options firmware_constraints)

add_executable(test_task4 tests/test_task4.cpp)
target_link_libraries(test_task4 PRIVATE task4_lib test_deps)
add_test(NAME task4 COMMAND test_task4)

# Size / speed benchmarks, built with -Os. Skipped under sanitizers (they distort timing).
# Run them with scripts/bench_task4.sh
if(NOT SANITIZE)
  add_executable(bench_task4_virtual bench/bench_virtual.cpp bench/virtual_factory.cpp)
  add_executable(bench_task4_crtp bench/bench_crtp.cpp)
  foreach(t bench_task4_virtual bench_task4_crtp)
    target_link_libraries(${t} PRIVATE task4_lib)
    target_compile_options(${t} PRIVATE -Os -ffunction-sections -fdata-sections)
    target_link_options(${t} PRIVATE -Wl,--gc-sections)
  endforeach()
endif()
