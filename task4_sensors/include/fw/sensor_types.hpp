#pragma once
// Task 4: types shared by both sensor designs.

#include <cstdint>
#include <optional>

namespace fw {

// One measurement. The unit depends on the sensor:
// temperature = deci-degrees Celsius, pressure = pascals.
struct Sample {
    std::int32_t value{};
};

constexpr bool operator==(Sample a, Sample b) { return a.value == b.value; }
constexpr bool operator!=(Sample a, Sample b) { return a.value != b.value; }

}  // namespace fw
