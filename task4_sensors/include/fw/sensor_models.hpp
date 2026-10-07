#pragma once
// Task 4: the mock sensor behaviour, shared by the virtual and the CRTP drivers.
// Keeping it in one place guarantees both designs do exactly the same work, so
// the benchmark measures dispatch cost and nothing else.
//
// The "hardware" is a small linear congruential generator: deterministic, so
// tests can predict it and the benchmark checksum can be compared.

#include <cstdint>
#include <optional>

#include "fw/sensor_types.hpp"

namespace fw {

class TemperatureModel {
public:
    explicit TemperatureModel(bool init_ok = true, std::uint32_t seed = 1)
        : init_ok_{init_ok}, state_{seed} {}

    bool init() {
        ready_ = init_ok_;
        return ready_;
    }

    std::optional<Sample> read() {
        if (!ready_) {
            return std::nullopt;
        }
        state_ = state_ * 1664525u + 1013904223u;
        // 20.0 .. 29.9 C, in deci-degrees
        return Sample{200 + static_cast<std::int32_t>((state_ >> 24) % 100u)};
    }

    static constexpr const char* name() { return "temperature"; }

private:
    bool init_ok_;
    bool ready_{false};
    std::uint32_t state_;
};

class PressureModel {
public:
    explicit PressureModel(bool init_ok = true, std::uint32_t seed = 2)
        : init_ok_{init_ok}, state_{seed} {}

    bool init() {
        ready_ = init_ok_;
        return ready_;
    }

    std::optional<Sample> read() {
        if (!ready_) {
            return std::nullopt;
        }
        state_ = state_ * 1664525u + 1013904223u;
        // 100000 .. 101999 Pa
        return Sample{100000 + static_cast<std::int32_t>((state_ >> 20) % 2000u)};
    }

    static constexpr const char* name() { return "pressure"; }

private:
    bool init_ok_;
    bool ready_{false};
    std::uint32_t state_;
};

}  // namespace fw
