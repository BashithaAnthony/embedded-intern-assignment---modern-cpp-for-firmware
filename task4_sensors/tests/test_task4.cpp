#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <type_traits>
#include <variant>

#include "fw/sensors.hpp"

using namespace fw;

static_assert(std::is_abstract_v<ISensor>, "ISensor is an abstract interface");
static_assert(!std::is_abstract_v<VirtualTemperatureSensor>, "concrete driver");
static_assert(std::is_polymorphic_v<VirtualTemperatureSensor>, "virtual driver has a vtable");
static_assert(!std::is_polymorphic_v<CrtpTemperatureSensor>, "CRTP driver has no vtable");

// Generic code over any CRTP sensor: resolved at compile time.
template <typename D>
std::int32_t read_value_or_minus_one(SensorBase<D>& s) {
    const std::optional<Sample> r = s.read();
    return r ? r->value : -1;
}

TEST_CASE("virtual: heterogeneous list of sensors through ISensor*") {
    VirtualTemperatureSensor temp;
    VirtualPressureSensor press;
    ISensor* list[] = {&temp, &press};

    for (ISensor* s : list) {
        CHECK(s->init());
        const std::optional<Sample> r = s->read();
        CHECK(r.has_value());
    }
    CHECK(std::strcmp(list[0]->name(), "temperature") == 0);
    CHECK(std::strcmp(list[1]->name(), "pressure") == 0);
}

TEST_CASE("CRTP: concrete sensors and generic code over SensorBase<D>") {
    CrtpTemperatureSensor temp;
    CrtpPressureSensor press;
    CHECK(temp.init());
    CHECK(press.init());
    CHECK(read_value_or_minus_one(temp) >= 200);
    CHECK(read_value_or_minus_one(press) >= 100000);
    CHECK(std::strcmp(temp.name(), "temperature") == 0);
    CHECK(std::strcmp(press.name(), "pressure") == 0);
}

TEST_CASE("read() before init() returns an empty optional (both designs)") {
    VirtualTemperatureSensor v;
    CrtpTemperatureSensor c;
    CHECK(!v.read().has_value());
    CHECK(!c.read().has_value());
    CHECK(v.init());
    CHECK(c.init());
    CHECK(v.read().has_value());
    CHECK(c.read().has_value());
}

TEST_CASE("a failing init() reports false and reads stay empty (both designs)") {
    VirtualPressureSensor v{false};
    CrtpPressureSensor c{false};
    CHECK(!v.init());
    CHECK(!c.init());
    CHECK(!v.read().has_value());
    CHECK(!c.read().has_value());
}

TEST_CASE("readings stay inside the expected physical range") {
    VirtualTemperatureSensor t;
    CrtpPressureSensor p;
    t.init();
    p.init();
    bool temp_ok = true;
    bool press_ok = true;
    for (int i = 0; i < 2000; ++i) {
        const auto a = t.read();
        const auto b = p.read();
        if (!a || a->value < 200 || a->value > 299) temp_ok = false;
        if (!b || b->value < 100000 || b->value > 101999) press_ok = false;
    }
    CHECK(temp_ok);
    CHECK(press_ok);
}

TEST_CASE("virtual and CRTP drivers produce identical sample streams") {
    VirtualTemperatureSensor vt{true, 7};
    CrtpTemperatureSensor ct{true, 7};
    VirtualPressureSensor vp{true, 9};
    CrtpPressureSensor cp{true, 9};
    vt.init();
    ct.init();
    vp.init();
    cp.init();

    bool same = true;
    for (int i = 0; i < 500; ++i) {
        if (vt.read() != ct.read()) same = false;
        if (vp.read() != cp.read()) same = false;
    }
    CHECK(same);
}

TEST_CASE("different seeds give different streams") {
    CrtpTemperatureSensor a{true, 1};
    CrtpTemperatureSensor b{true, 12345};
    a.init();
    b.init();
    bool differ = false;
    for (int i = 0; i < 50; ++i) {
        if (a.read() != b.read()) differ = true;
    }
    CHECK(differ);
}

TEST_CASE("CRTP heterogeneous list: closed set of types in a std::variant") {
    using AnySensor = std::variant<CrtpTemperatureSensor, CrtpPressureSensor>;
    AnySensor list[] = {CrtpTemperatureSensor{}, CrtpPressureSensor{}};

    unsigned ok = 0;
    for (AnySensor& s : list) {
        std::visit(
            [&ok](auto& sensor) {
                if (sensor.init() && sensor.read().has_value()) ++ok;
            },
            s);
    }
    CHECK(ok == 2u);
}

TEST_CASE("per-object cost: the virtual driver carries a vptr, the CRTP driver does not") {
    CHECK(sizeof(VirtualTemperatureSensor) >= sizeof(CrtpTemperatureSensor) + sizeof(void*));
    CHECK(sizeof(VirtualPressureSensor) >= sizeof(CrtpPressureSensor) + sizeof(void*));
    CHECK(sizeof(CrtpTemperatureSensor) == sizeof(TemperatureModel));  // empty base adds nothing
}
