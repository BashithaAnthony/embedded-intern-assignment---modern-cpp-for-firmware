#pragma once
// Task 1: strongly typed physical quantities.
// millivolts, milliamps, milliseconds and deci-degrees as different types.

#include <cstdint>
#include <type_traits>

namespace fw {

template <typename Tag, typename Rep>
class Quantity {
    static_assert(std::is_integral_v<Rep>, "Quantity: Rep must be an integer type");

public:
    using rep = Rep;

    // --- construction and access ------------------------------------------
    constexpr Quantity() = default;
    constexpr explicit Quantity(Rep value) : value_{value} {}

    constexpr Rep count() const { return value_; }

    // --- compound assignment (members: they modify *this) -----------------
    constexpr Quantity& operator+=(Quantity rhs) {
        value_ = static_cast<Rep>(value_ + rhs.value_);
        return *this;
    }
    constexpr Quantity& operator-=(Quantity rhs) {
        value_ = static_cast<Rep>(value_ - rhs.value_);
        return *this;
    }
    constexpr Quantity& operator*=(Rep k) {
        value_ = static_cast<Rep>(value_ * k);
        return *this;
    }
    constexpr Quantity& operator/=(Rep k) {
        value_ = static_cast<Rep>(value_ / k);
        return *this;
    }

    // --- binary / unary arithmetic (hidden friends) ------------------------
    friend constexpr Quantity operator+(Quantity a, Quantity b) { a += b; return a; }
    friend constexpr Quantity operator-(Quantity a, Quantity b) { a -= b; return a; }
    friend constexpr Quantity operator-(Quantity a) { return Quantity{static_cast<Rep>(-a.value_)}; }

    friend constexpr Quantity operator*(Quantity a, Rep k) { a *= k; return a; }
    friend constexpr Quantity operator*(Rep k, Quantity a) { a *= k; return a; }
    friend constexpr Quantity operator/(Quantity a, Rep k) { a /= k; return a; }

    // --- comparison (C++17: all six written out) ---------------------------
    friend constexpr bool operator==(Quantity a, Quantity b) { return a.value_ == b.value_; }
    friend constexpr bool operator!=(Quantity a, Quantity b) { return a.value_ != b.value_; }
    friend constexpr bool operator<(Quantity a, Quantity b) { return a.value_ < b.value_; }
    friend constexpr bool operator<=(Quantity a, Quantity b) { return a.value_ <= b.value_; }
    friend constexpr bool operator>(Quantity a, Quantity b) { return a.value_ > b.value_; }
    friend constexpr bool operator>=(Quantity a, Quantity b) { return a.value_ >= b.value_; }

private:
    Rep value_{};
};

// --- tags: empty types that exist only to make each unit a distinct type ---
struct MillivoltTag {};
struct MilliampTag {};
struct MillisecondTag {};
struct DeciCelsiusTag {};

// --- the units -------------------------------------------------------------
using Millivolts   = Quantity<MillivoltTag, std::int32_t>;
using Milliamps    = Quantity<MilliampTag, std::int32_t>;
using Milliseconds = Quantity<MillisecondTag, std::int32_t>;
using DeciCelsius  = Quantity<DeciCelsiusTag, std::int16_t>;  // 253 == 25.3 C

// --- zero-overhead proof ----------------------------------------------------
static_assert(sizeof(Millivolts) == sizeof(std::int32_t), "Millivolts must be 4 bytes");
static_assert(sizeof(Milliamps) == sizeof(std::int32_t), "Milliamps must be 4 bytes");
static_assert(sizeof(Milliseconds) == sizeof(std::int32_t), "Milliseconds must be 4 bytes");
static_assert(sizeof(DeciCelsius) == sizeof(std::int16_t), "DeciCelsius must be 2 bytes");
static_assert(std::is_trivially_copyable_v<Millivolts>, "must be copyable like an int");
static_assert(std::is_standard_layout_v<Millivolts>, "must have plain int layout");

// --- user-defined literals ---------------------------------------------------
namespace literals {

constexpr Millivolts operator""_mV(unsigned long long v) {
    return Millivolts{static_cast<std::int32_t>(v)};
}
constexpr Milliamps operator""_mA(unsigned long long v) {
    return Milliamps{static_cast<std::int32_t>(v)};
}
constexpr Milliseconds operator""_ms(unsigned long long v) {
    return Milliseconds{static_cast<std::int32_t>(v)};
}
constexpr DeciCelsius operator""_dC(unsigned long long v) {
    return DeciCelsius{static_cast<std::int16_t>(v)};
}

}  // namespace literals

// --- ADC conversion ----------------------------------------------------------
inline constexpr std::int32_t kAdc12MaxCount = 4095;       // 2^12 - 1
inline constexpr std::int32_t kAdcVrefMillivolts = 3300;   // 3.3 V reference

// counts * 3300 / 4095, rounded to nearest, using integers only.
// Inputs above full scale are clamped to 4095.
constexpr Millivolts adc12_to_millivolts(std::uint16_t counts) {
    const std::int32_t c =
        (counts > kAdc12MaxCount) ? kAdc12MaxCount : static_cast<std::int32_t>(counts);
    const std::int32_t mv =
        (2 * c * kAdcVrefMillivolts + kAdc12MaxCount) / (2 * kAdc12MaxCount);
    return Millivolts{mv};
}

}  // namespace fw
