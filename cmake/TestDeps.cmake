# doctest: single-header test framework, fetched at configure time.
include(FetchContent)
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.11)
FetchContent_MakeAvailable(doctest)

# SYSTEM include so -Werror never fires on third-party headers.
add_library(test_deps INTERFACE)
target_include_directories(test_deps SYSTEM INTERFACE ${doctest_SOURCE_DIR})
