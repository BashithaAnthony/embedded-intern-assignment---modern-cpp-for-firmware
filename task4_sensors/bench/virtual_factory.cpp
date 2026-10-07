// Defined in its own translation unit so the compiler cannot see the concrete
// types in bench_virtual.cpp and turn the virtual calls into direct calls.
// That mirrors firmware where the driver is picked at run time.
#include "fw/sensor_virtual.hpp"

namespace fw {

namespace {
VirtualTemperatureSensor g_temperature;
VirtualPressureSensor g_pressure;
}  // namespace

ISensor& bench_sensor(unsigned index) {
    if (index == 0) {
        return g_temperature;
    }
    return g_pressure;
}

}  // namespace fw
