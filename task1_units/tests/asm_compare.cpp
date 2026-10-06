// NOT built by CMake. Paste into Compiler Explorer (godbolt.org) with -O2
// (try x86-64 gcc, then "ARM GCC" with -mcpu=cortex-m4 -mthumb) after
// pasting include/fw/units.hpp above it. The two functions must produce the same instructions.
#include "fw/units.hpp"
using namespace fw;

std::int32_t raw_fn(std::int32_t a, std::int32_t b) { return (a + b) * 3 - 5; }
Millivolts qty_fn(Millivolts a, Millivolts b) { return (a + b) * 3 - Millivolts{5}; }
