#pragma once
// Task 4, design 1: dynamic polymorphism (virtual functions).
// One abstract interface; callers hold ISensor* and the call is resolved at run time
// through the object's vtable pointer.

#include <cstdint>
#include <optional>

#include "fw/sensor_models.hpp"
#include "fw/sensor_types.hpp"

namespace fw {

class ISensor {
public:
    ISensor(const ISensor&) = delete;
    ISensor& operator=(const ISensor&) = delete;

    virtual bool init() = 0;
    virtual std::optional<Sample> read() = 0;
    virtual const char* name() const = 0;

protected:
    ISensor() = default;
    ~ISensor() = default;  // never deleted through a base pointer (no heap here)
};

class VirtualTemperatureSensor final : public ISensor {
public:
    explicit VirtualTemperatureSensor(bool init_ok = true, std::uint32_t seed = 1)
        : model_{init_ok, seed} {}

    bool init() override { return model_.init(); }
    std::optional<Sample> read() override { return model_.read(); }
    const char* name() const override { return TemperatureModel::name(); }

private:
    TemperatureModel model_;
};

class VirtualPressureSensor final : public ISensor {
public:
    explicit VirtualPressureSensor(bool init_ok = true, std::uint32_t seed = 2)
        : model_{init_ok, seed} {}

    bool init() override { return model_.init(); }
    std::optional<Sample> read() override { return model_.read(); }
    const char* name() const override { return PressureModel::name(); }

private:
    PressureModel model_;
};

}  // namespace fw
