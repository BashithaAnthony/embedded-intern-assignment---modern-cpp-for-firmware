#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>
#include <type_traits>
#include <utility>

#include "fw/units.hpp"

using namespace fw;
using namespace fw::literals;

// --- compile-time detection: "does A + B compile?" ---------------------------
template <typename A, typename B, typename = void>
struct can_add : std::false_type {};
template <typename A, typename B>
struct can_add<A, B, std::void_t<decltype(std::declval<A>() + std::declval<B>())>>
    : std::true_type {};

template <typename A, typename B, typename = void>
struct can_compare : std::false_type {};
template <typename A, typename B>
struct can_compare<A, B, std::void_t<decltype(std::declval<A>() == std::declval<B>())>>
    : std::true_type {};

static_assert(can_add<Millivolts, Millivolts>::value, "mV + mV must work");
static_assert(!can_add<Millivolts, Milliseconds>::value, "mV + ms must NOT compile");
static_assert(!can_add<Millivolts, Milliamps>::value, "mV + mA must NOT compile");
static_assert(!can_add<Millivolts, std::int32_t>::value, "mV + raw int must NOT compile");
static_assert(!can_compare<Millivolts, Milliseconds>::value, "mV == ms must NOT compile");
static_assert(!std::is_convertible_v<std::int32_t, Millivolts>, "int -> mV must be explicit");
static_assert(!std::is_same_v<Millivolts, Milliamps>, "units must be distinct types");

TEST_CASE("construct, read back, default is zero") {
    constexpr Millivolts v{3300};
    static_assert(v.count() == 3300, "constexpr read-back");
    CHECK(v.count() == 3300);
    constexpr Millivolts zero{};
    CHECK(zero.count() == 0);
}

TEST_CASE("addition, subtraction, negation") {
    constexpr Millivolts a = 3000_mV;
    constexpr Millivolts b = 300_mV;
    static_assert((a + b).count() == 3300, "constexpr add");
    CHECK((a + b).count() == 3300);
    CHECK((a - b).count() == 2700);
    CHECK((b - a).count() == -2700);
    CHECK((-a).count() == -3000);
}

TEST_CASE("compound assignment") {
    Milliamps i = 100_mA;
    i += 50_mA;
    CHECK(i.count() == 150);
    i -= 30_mA;
    CHECK(i.count() == 120);
    i *= 2;
    CHECK(i.count() == 240);
    i /= 4;
    CHECK(i.count() == 60);
}

TEST_CASE("scaling by a plain number, both orders") {
    constexpr Milliseconds t = 250_ms;
    static_assert((t * 4).count() == 1000, "constexpr scale");
    CHECK((t * 4).count() == 1000);
    CHECK((4 * t).count() == 1000);
    CHECK((t / 5).count() == 50);
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
    static_assert((3300_mV).count() == 3300, "");
    static_assert((250_ms).count() == 250, "");
    static_assert((20_mA).count() == 20, "");
    static_assert((253_dC).count() == 253, "");
    CHECK((3300_mV).count() == 3300);
    CHECK((250_ms).count() == 250);
}

TEST_CASE("12-bit ADC to millivolts (3.3 V reference)") {
    CHECK(adc12_to_millivolts(0).count() == 0);
    CHECK(adc12_to_millivolts(4095).count() == 3300);
    CHECK(adc12_to_millivolts(2048).count() == 1650);
    CHECK(adc12_to_millivolts(1).count() == 1);      // 0.806 mV rounds to 1
    CHECK(adc12_to_millivolts(5000).count() == 3300);  // clamped to full scale
    static_assert(adc12_to_millivolts(4095).count() == 3300, "usable at compile time");
}

TEST_CASE("zero overhead: same size and layout as the raw integer") {
    static_assert(sizeof(Millivolts) == sizeof(std::int32_t), "");
    static_assert(sizeof(DeciCelsius) == sizeof(std::int16_t), "");
    static_assert(std::is_trivially_copyable_v<Millivolts>, "");
    CHECK(sizeof(Millivolts) == 4);
}
