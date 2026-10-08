#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "fw/units.hpp"

using namespace fw;
using namespace fw::literals;

TEST_CASE("construct, read back, default is zero") {
    constexpr Millivolts v{3300};
    static_assert(v.count() == 3300, "usable at compile time");
    CHECK(v.count() == 3300);
    CHECK(Millivolts{}.count() == 0);
}

TEST_CASE("addition and subtraction") {
    constexpr Millivolts a = 3000_mV;
    constexpr Millivolts b = 300_mV;
    static_assert((a + b).count() == 3300, "usable at compile time");
    CHECK((a + b).count() == 3300);
    CHECK((a - b).count() == 2700);
    CHECK((b - a).count() == -2700);
}

TEST_CASE("scaling by a plain number, in both orders") {
    CHECK((250_ms * 4).count() == 1000);
    CHECK((4 * 250_ms).count() == 1000);
    CHECK((DeciCelsius{253} * 2).count() == 506);
}

TEST_CASE("comparisons") {
    CHECK((1_mV < 2_mV));
    CHECK((2_mV <= 2_mV));
    CHECK((3_mV > 2_mV));
    CHECK((3_mV >= 3_mV));
    CHECK((2_mV == 2_mV));
    CHECK((2_mV != 3_mV));
}

TEST_CASE("user-defined literals") {
    CHECK((3300_mV).count() == 3300);
    CHECK((20_mA).count() == 20);
    CHECK((250_ms).count() == 250);
}

TEST_CASE("12-bit ADC to millivolts") {
    CHECK(adc12_to_millivolts(0).count() == 0);
    CHECK(adc12_to_millivolts(1).count() == 1);        // 0.806 mV rounds to 1
    CHECK(adc12_to_millivolts(2048).count() == 1650);
    CHECK(adc12_to_millivolts(4095).count() == 3300);
    CHECK(adc12_to_millivolts(5000).count() == 3300);  // above full scale: clamped
}
