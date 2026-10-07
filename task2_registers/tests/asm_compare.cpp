// NOT built by CMake. Paste fw/registers.hpp first, then this file, into
// Compiler Explorer (godbolt.org) at -O2. Try x86-64 gcc and "ARM GCC"
// with -mcpu=cortex-m4 -mthumb. The two functions must compile to the same
// load / modify / store sequence.
#include "fw/registers.hpp"
using namespace fw;

using Moder = Register<0x40020000, Access::ReadWrite>;
using Pin5Mode = Field<Moder, 10, 2, GpioMode>;

void field_version() { Pin5Mode::write(GpioMode::Output); }

void handwritten_version() {
    volatile std::uint32_t* r = reinterpret_cast<volatile std::uint32_t*>(0x40020000);
    *r = (*r & ~(0x3u << 10)) | (0x1u << 10);
}
