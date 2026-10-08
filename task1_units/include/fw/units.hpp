#pragma once
// Task 1: strongly typed physical quantities.
// A Quantity is just an integer at run time, but the compiler treats millivolts,
// milliamps, milliseconds and deci-degrees as different types.

#include <cstdint>

namespace fw {

template <typename Tag, typename Rep>
class Quantity {
public:
    constexpr Quantity() = default;
    constexpr explicit Quantity(Rep value) : value_{value} {}  // explicit: no silent int -> Quantity

    constexpr Rep count() const { return value_; }

    // so Millivolts + Milliseconds has no matching operator and does not compile.
    friend constexpr Quantity operator+(Quantity a, Quantity b) {
        return Quantity{static_cast<Rep>(a.value_ + b.value_)};
    }
    friend constexpr Quantity operator-(Quantity a, Quantity b) {
        return Quantity{static_cast<Rep>(a.value_ - b.value_)};
    }

    // Scaling by a plain number
    friend constexpr Quantity operator*(Quantity a, Rep k) {
        return Quantity{static_cast<Rep>(a.value_ * k)};
    }
    friend constexpr Quantity operator*(Rep k, Quantity a) { return a * k; }

    // Comparison (C++17: all six are written out)
    friend constexpr bool operator==(Quantity a, Quantity b) { return a.value_ == b.value_; }
    friend constexpr bool operator!=(Quantity a, Quantity b) { return a.value_ != b.value_; }
    friend constexpr bool operator<(Quantity a, Quantity b) { return a.value_ < b.value_; }
    friend constexpr bool operator<=(Quantity a, Quantity b) { return a.value_ <= b.value_; }
    friend constexpr bool operator>(Quantity a, Quantity b) { return a.value_ > b.value_; }
    friend constexpr bool operator>=(Quantity a, Quantity b) { return a.value_ >= b.value_; }

private:
    Rep value_{};
};

// Tags: empty types that only make each unit a distinct type.
struct MillivoltTag {};
struct MilliampTag {};
struct MillisecondTag {};
struct DeciCelsiusTag {};

using Millivolts   = Quantity<MillivoltTag, std::int32_t>;
using Milliamps    = Quantity<MilliampTag, std::int32_t>;
using Milliseconds = Quantity<MillisecondTag, std::int32_t>;
using DeciCelsius  = Quantity<DeciCelsiusTag, std::int16_t>;  // 253 means 25.3 C

// Zero overhead: a Millivolts is exactly as big as the integer inside it.
static_assert(sizeof(Millivolts) == sizeof(std::int32_t), "Millivolts must be 4 bytes");

// User-defined literals: 3300_mV, 20_mA, 250_ms
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
}  // namespace literals

// 12-bit ADC with a 3.3 V reference: counts * 3300 / 4095, rounded to nearest.
// Counts above 4095 are clamped to full scale.
constexpr Millivolts adc12_to_millivolts(std::uint16_t counts) {
    const std::int32_t c = counts > 4095 ? 4095 : counts;
    return Millivolts{(2 * c * 3300 + 4095) / (2 * 4095)};
}

}  // namespace fw
