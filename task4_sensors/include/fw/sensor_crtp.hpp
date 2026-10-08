#pragma once
// Task 4, design 2: static polymorphism (CRTP, the Curiously Recurring Template Pattern).
// SensorBase<Derived> calls Derived::do_init(), do_read() and do_name(). The call is
// resolved at compile time: no vtable, no vptr, and the compiler can inline it.

#include <optional>

#include "fw/sensor_models.hpp"

namespace fw {

template <typename Derived>
class SensorBase {
public:
    bool init() { return self().do_init(); }
    std::optional<Sample> read() { return self().do_read(); }
    const char* name() const { return self().do_name(); }

private:
    Derived& self() { return static_cast<Derived&>(*this); }
    const Derived& self() const { return static_cast<const Derived&>(*this); }
};

class CrtpTemperatureSensor : public SensorBase<CrtpTemperatureSensor> {
public:
    bool do_init() { return model_.init(); }
    std::optional<Sample> do_read() { return model_.read(); }
    const char* do_name() const { return model_.name(); }

private:
    TemperatureModel model_;
};

class CrtpPressureSensor : public SensorBase<CrtpPressureSensor> {
public:
    bool do_init() { return model_.init(); }
    std::optional<Sample> do_read() { return model_.read(); }
    const char* do_name() const { return model_.name(); }

private:
    PressureModel model_;
};

}  // namespace fw
