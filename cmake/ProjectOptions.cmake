# Sanitizer selection:  -DSANITIZE=address,undefined   or   -DSANITIZE=thread
set(SANITIZE "" CACHE STRING "Value passed to -fsanitize= (empty = off)")

# Warnings + sanitizers, applied to every target that links project_options.
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_17)
target_compile_options(project_options INTERFACE -Wall -Wextra -Wpedantic -Werror)

if(SANITIZE)
  target_compile_options(project_options INTERFACE
    -fsanitize=${SANITIZE} -fno-omit-frame-pointer)
  target_link_options(project_options INTERFACE -fsanitize=${SANITIZE})
endif()

# Tasks 1-4 must build without exceptions and RTTI.
add_library(firmware_constraints INTERFACE)
target_compile_options(firmware_constraints INTERFACE -fno-exceptions -fno-rtti)
if(SANITIZE MATCHES "undefined")
  # UBSan's vptr check needs RTTI, which Tasks 1-4 deliberately disable.
  target_compile_options(firmware_constraints INTERFACE -fno-sanitize=vptr)
endif()
