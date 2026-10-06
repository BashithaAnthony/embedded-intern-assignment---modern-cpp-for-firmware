// Task 1 report item: expressions that correctly FAIL to compile.
// This file is NOT part of the build. To check one, copy it out of the
// #if 0 block (together with the includes) and compile it: you must see an error.
#if 0
#include "fw/units.hpp"
using namespace fw;
using namespace fw::literals;

void examples() {
    // 1. Different units cannot be added: no operator+ accepts (Millivolts, Milliseconds).
    auto bad1 = 3300_mV + 250_ms;

    // 2. A raw integer is not a Millivolts: the constructor is explicit.
    Millivolts bad2 = 5;

    // 3. Adding a bare number to a quantity: there is no implicit int -> Millivolts conversion.
    auto bad3 = 3300_mV + 5;

    // 4. Comparing different units: no operator== accepts (Millivolts, Milliamps).
    bool bad4 = (3300_mV == 20_mA);

    // 5. Passing the wrong unit to a function that expects Millivolts.
    //    void set_dac(Millivolts); set_dac(250_ms);

    // 6. Multiplying two quantities (mV * mV would be mV^2, not a Millivolts):
    //    the scalar overload needs a plain integer, not a Quantity.
    auto bad6 = 3300_mV * 2_mV;

    // 7. Assigning one unit to another:
    Milliamps bad7 = 3300_mV;
}
#endif
