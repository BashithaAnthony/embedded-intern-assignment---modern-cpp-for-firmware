#pragma once
// Task 4: the sample type and the mock sensor behaviour, shared by both designs.
// Both the virtual and the CRTP drivers forward to these classes, so they do exactly
// the same work and the benchmark measures only the cost of the call.
// The "hardware" is a small random number generator, so the readings are repeatable.

#include <cstdint>
#include <optional>

namespace fw {

// One measurement. Temperature is in deci-degrees Celsius, pressure in pascals.
struct Sample {
    std::int32_t value;
};

class TemperatureModel {
public:
    bool init() {
        ready_ = true;
        return true;
    }

    // Empty until init() has been called.
    std::optional<Sample> read() {
        if (!ready_) {
            return std::nullopt;
        }
        state_ = state_ * 1664525u + 1013904223u;
        return Sample{200 + static_cast<std::int32_t>((state_ >> 24) % 100u)};  // 20.0 .. 29.9 C
    }

    static const char* name() { return "temperature"; }

private:
    bool ready_ = false;
    std::uint32_t state_ = 1;
};

class PressureModel {
public:
    bool init() {
        ready_ = true;
        return true;
    }

    std::optional<Sample> read() {
        if (!ready_) {
            return std::nullopt;
        }
        state_ = state_ * 1664525u + 1013904223u;
        return Sample{100000 + static_cast<std::int32_t>((state_ >> 20) % 2000u)};  // about 1000 hPa
    }

    static const char* name() { return "pressure"; }

private:
    bool ready_ = false;
    std::uint32_t state_ = 2;
};

}  // namespace fw
