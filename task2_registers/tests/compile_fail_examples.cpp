// Task 2 report item: mistakes that must FAIL to compile.
// NOT part of the build. To check one, copy it (with the includes and the aliases)
// into its own file and compile it: you must see an error.
#if 0
#include "fw/registers.hpp"
using namespace fw;

using RwReg = Register<0x40020000, Access::ReadWrite>;
using RoReg = Register<0x40020004, Access::ReadOnly>;
using WoReg = Register<0x40020008, Access::WriteOnly>;
using Pin5Mode = Field<RwReg, 10, 2, GpioMode>;

void examples() {
    // 1. Field runs past bit 31 (offset 30 + width 4 = 34).
    constexpr auto m = Field<RwReg, 30, 4>::mask;     // static_assert: overflows 32 bits

    // 2. Writing a read-only register.
    RoReg::write(1);                                  // static_assert: read-only

    // 3. Writing a field of a read-only register.
    Field<RoReg, 0, 1>::set();                        // static_assert: read-only

    // 4. Reading a write-only register.
    auto v = WoReg::read();                           // static_assert: write-only

    // 5. A raw integer where an enum class is required.
    Pin5Mode::write(1);                               // no conversion from int to GpioMode

    // 6. set() on a multi-bit field.
    Pin5Mode::set();                                  // static_assert: 1-bit fields only
}
#endif
