#pragma once
// Task 4, design 1: dynamic polymorphism (virtual functions).
// Callers hold an ISensor*, and each call is resolved at run time through the vtable.

#include <optional>

#include "fw/sensor_models.hpp"

namespace fw {

class ISensor {
public:
    virtual bool init() = 0;
    virtual std::optional<Sample> read() = 0;
    virtual const char* name() const = 0;

protected:
    ~ISensor() = default;  // nobody deletes through an ISensor*, so no virtual destructor
};

class VirtualTemperatureSensor : public ISensor {
public:
    bool init() override { return model_.init(); }
    std::optional<Sample> read() override { return model_.read(); }
    const char* name() const override { return model_.name(); }

private:
    TemperatureModel model_;
};

class VirtualPressureSensor : public ISensor {
public:
    bool init() override { return model_.init(); }
    std::optional<Sample> read() override { return model_.read(); }
    const char* name() const override { return model_.name(); }

private:
    PressureModel model_;
};

}  // namespace fw
