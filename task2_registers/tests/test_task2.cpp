#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>

#include "fw/registers.hpp"

using namespace fw;

// Storage policy backed by a plain array, so the tests run on a PC.
struct Sim {
    static constexpr std::uintptr_t kBase = 0x1000;  // fake peripheral address
    inline static std::uint32_t mem[3]{};

    static std::uint32_t load(std::uintptr_t a) { return mem[(a - kBase) / 4]; }
    static void store(std::uintptr_t a, std::uint32_t v) { mem[(a - kBase) / 4] = v; }
    static void reset() {
        for (auto& w : mem) w = 0;
    }
};

using Moder  = Register<Sim::kBase + 0, Access::ReadWrite, Sim>;  // GPIO mode register
using Status = Register<Sim::kBase + 4, Access::ReadOnly, Sim>;   // read-only flags
using Cfg    = Register<Sim::kBase + 8, Access::ReadWrite, Sim>;  // configuration

using Pin5Mode  = Field<Moder, 10, 2, GpioMode>;  // 2-bit field at bit 10
using Ready     = Field<Status, 3, 1>;            // 1-bit field at bit 3
using Enable    = Field<Cfg, 0, 1>;               // 1-bit field at bit 0
using Prescaler = Field<Cfg, 4, 3>;               // 3-bit number at bit 4

TEST_CASE("register read and write go through the storage policy") {
    Sim::reset();
    Moder::write(0xA5A5A5A5u);
    CHECK(Sim::mem[0] == 0xA5A5A5A5u);
    CHECK(Moder::read() == 0xA5A5A5A5u);
}

TEST_CASE("the field mask is computed at compile time") {
    static_assert(Pin5Mode::mask == 0xC00u, "bits 11:10");
    static_assert(Prescaler::mask == 0x70u, "bits 6:4");
    CHECK(Pin5Mode::mask == 0xC00u);
}

TEST_CASE("field write changes only its own bits") {
    Sim::reset();
    Sim::mem[0] = 0xFFFFFFFFu;
    Pin5Mode::write(GpioMode::Output);  // bits 11:10 become 01
    CHECK(Sim::mem[0] == 0xFFFFF7FFu);
    Pin5Mode::write(GpioMode::Input);   // bits 11:10 become 00
    CHECK(Sim::mem[0] == 0xFFFFF3FFu);
}

TEST_CASE("field read returns the shifted value") {
    Sim::reset();
    Sim::mem[0] = 0x800u;  // bits 11:10 = 10
    CHECK((Pin5Mode::read() == GpioMode::AltFunc));
    Sim::mem[1] = 0x8u;    // bit 3 of the read-only status register
    CHECK(Ready::read() == 1u);
}

TEST_CASE("set() and clear() change a single bit") {
    Sim::reset();
    Sim::mem[2] = 0xF0F0F0F0u;
    Enable::set();
    CHECK(Sim::mem[2] == 0xF0F0F0F1u);
    Enable::clear();
    CHECK(Sim::mem[2] == 0xF0F0F0F0u);
}

TEST_CASE("an oversized value is cut off and cannot spill into neighbours") {
    Sim::reset();
    Prescaler::write(0xFFu);  // only 3 bits fit
    CHECK(Sim::mem[2] == 0x70u);
}
