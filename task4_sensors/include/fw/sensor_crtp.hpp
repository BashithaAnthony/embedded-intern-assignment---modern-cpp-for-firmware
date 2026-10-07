#pragma once
// Task 4, design 2: static polymorphism (CRTP, the Curiously Recurring Template Pattern).
// SensorBase<Derived> forwards to Derived::do_init() / do_read() / do_name(), resolved
// at compile time: no vtable, no vptr in the objects, and every call can be inlined.

#include <cstdint>
#include <optional>

#include "fw/sensor_models.hpp"
#include "fw/sensor_types.hpp"

namespace fw {

template <typename Derived>
class SensorBase {
public:
    bool init() { return self().do_init(); }
    std::optional<Sample> read() { return self().do_read(); }
    const char* name() const { return self().do_name(); }

private:
    // Private constructor + friend Derived: only the class named as Derived can
    // inherit from SensorBase<Derived>. "class A : SensorBase<B>" will not compile.
    SensorBase() = default;
    ~SensorBase() = default;
    friend Derived;

    Derived& self() { return static_cast<Derived&>(*this); }
    const Derived& self() const { return static_cast<const Derived&>(*this); }
};

class CrtpTemperatureSensor final : public SensorBase<CrtpTemperatureSensor> {
public:
    explicit CrtpTemperatureSensor(bool init_ok = true, std::uint32_t seed = 1)
        : model_{init_ok, seed} {}

private:
    friend class SensorBase<CrtpTemperatureSensor>;
    bool do_init() { return model_.init(); }
    std::optional<Sample> do_read() { return model_.read(); }
    const char* do_name() const { return TemperatureModel::name(); }

    TemperatureModel model_;
};

class CrtpPressureSensor final : public SensorBase<CrtpPressureSensor> {
public:
    explicit CrtpPressureSensor(bool init_ok = true, std::uint32_t seed = 2)
        : model_{init_ok, seed} {}

private:
    friend class SensorBase<CrtpPressureSensor>;
    bool do_init() { return model_.init(); }
    std::optional<Sample> do_read() { return model_.read(); }
    const char* do_name() const { return PressureModel::name(); }

    PressureModel model_;
};

}  // namespace fw
