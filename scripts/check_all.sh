#!/usr/bin/env bash
# Configure, build and test with GCC and Clang, each plain / ASan+UBSan / TSan.
set -euo pipefail
cd "$(dirname "$0")/.."

for cxx in g++ clang++; do
  command -v "$cxx" >/dev/null || { echo "== skipping $cxx (not installed)"; continue; }
  for san in "" "address,undefined" "thread"; do
    dir="build/${cxx}-${san:-plain}"; dir="${dir//,/_}"
    echo "== $cxx  sanitize='${san}'  -> $dir"
    cmake -S . -B "$dir" -DCMAKE_CXX_COMPILER="$cxx" -DSANITIZE="$san" -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$dir" -j
    ctest --test-dir "$dir" --output-on-failure
  done
done
echo "All configurations passed."
