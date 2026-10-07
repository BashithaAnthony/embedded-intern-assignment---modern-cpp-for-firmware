// Task 2 report item: mistakes that must FAIL to compile.
// NOT part of the build. To check one, copy the includes plus that single
// snippet into its own file and compile it: you must see an error.
#if 0
#include "fw/registers.hpp"
using namespace fw;

using RwReg = Register<0x40020000, Access::ReadWrite>;
using RoReg = Register<0x40020004, Access::ReadOnly>;
using WoReg = Register<0x40020008, Access::WriteOnly>;
using Pin5Mode = Field<RwReg, 10, 2, GpioMode>;

void examples() {
    // 1. Field runs past bit 31 (offset 30 + width 4 = 34).
    constexpr auto m1 = Field<RwReg, 30, 4>::mask;           // static_assert: overflows 32 bits

    // 2. Zero-width field.
    constexpr auto m2 = Field<RwReg, 0, 0>::mask;            // static_assert: width must be >= 1

    // 3. Writing a read-only register.
    RoReg::write(1);                                         // static_assert: read-only

    // 4. Writing a field of a read-only register.
    Field<RoReg, 0, 1>::set();                               // static_assert: read-only

    // 5. Reading a write-only register.
    auto v5 = WoReg::read();                                 // static_assert: write-only

    // 6. A field write needs a read-modify-write, impossible on write-only registers.
    Field<WoReg, 0, 1>::set();                               // static_assert: must be readable

    // 7. A raw integer where an enum class is required.
    Pin5Mode::write(1);                                      // no conversion from int to GpioMode

    // 8. The wrong enum type.
    enum class Other { A };
    Pin5Mode::write(Other::A);                               // no conversion between enum classes

    // 9. set() on a multi-bit field.
    Pin5Mode::set();                                         // static_assert: 1-bit fields only

    // 10. A value that does not fit, checked at compile time.
    Field<RwReg, 4, 3>::write_const<8u>();                   // static_assert: does not fit
}
#endif
