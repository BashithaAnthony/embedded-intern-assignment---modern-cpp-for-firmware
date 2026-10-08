// Task 1 report item: expressions that correctly FAIL to compile.
// NOT part of the build. To check one, copy it (with the includes) into its own file
// and compile it: you must see an error.
#if 0
#include "fw/units.hpp"
using namespace fw;
using namespace fw::literals;

void set_dac(Millivolts);

void examples() {
    // 1. Different units cannot be added: no operator+ accepts (Millivolts, Milliseconds).
    auto bad1 = 3300_mV + 250_ms;

    // 2. A raw integer is not a Millivolts: the constructor is explicit.
    Millivolts bad2 = 5;

    // 3. Adding a bare number: there is no implicit int -> Millivolts conversion.
    auto bad3 = 3300_mV + 5;

    // 4. Comparing different units: no operator== accepts (Millivolts, Milliamps).
    bool bad4 = (3300_mV == 20_mA);

    // 5. Passing the wrong unit to a function that expects Millivolts.
    set_dac(250_ms);
}
#endif
