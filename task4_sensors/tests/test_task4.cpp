#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstring>
#include <optional>

#include "fw/sensor_crtp.hpp"
#include "fw/sensor_virtual.hpp"

using namespace fw;

TEST_CASE("virtual: a list of different sensors through ISensor*") {
    VirtualTemperatureSensor temperature;
    VirtualPressureSensor pressure;
    ISensor* sensors[] = {&temperature, &pressure};  // one list, two different types

    for (ISensor* s : sensors) {
        CHECK(s->init());
        CHECK(s->read().has_value());
    }
    CHECK(std::strcmp(sensors[0]->name(), "temperature") == 0);
    CHECK(std::strcmp(sensors[1]->name(), "pressure") == 0);
}

TEST_CASE("CRTP: concrete sensors, resolved at compile time") {
    CrtpTemperatureSensor temperature;
    CrtpPressureSensor pressure;
    CHECK(temperature.init());
    CHECK(pressure.init());
    CHECK(temperature.read().has_value());
    CHECK(pressure.read().has_value());
    CHECK(std::strcmp(temperature.name(), "temperature") == 0);
    CHECK(std::strcmp(pressure.name(), "pressure") == 0);
}

TEST_CASE("virtual: read() before init() gives an empty optional") {
    VirtualTemperatureSensor sensor;
    CHECK(!sensor.read().has_value());
    sensor.init();
    CHECK(sensor.read().has_value());
}

TEST_CASE("CRTP: read() before init() gives an empty optional") {
    CrtpTemperatureSensor sensor;
    CHECK(!sensor.read().has_value());
    sensor.init();
    CHECK(sensor.read().has_value());
}

TEST_CASE("both designs give the same readings") {
    VirtualPressureSensor v;
    CrtpPressureSensor c;
    v.init();
    c.init();
    bool same = true;
    for (int i = 0; i < 100; ++i) {
        if (v.read()->value != c.read()->value) same = false;
    }
    CHECK(same);
}

TEST_CASE("a virtual sensor is bigger: it carries a vptr") {
    CHECK(sizeof(VirtualTemperatureSensor) > sizeof(CrtpTemperatureSensor));
}
