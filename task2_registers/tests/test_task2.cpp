#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>
#include <type_traits>
#include <utility>

#include "fw/registers.hpp"

using namespace fw;

// --- A storage policy backed by a plain array, so tests run on a PC ----------
struct Sim {
    static constexpr std::uintptr_t kBase = 0x1000;  // fake "peripheral" address
    inline static std::uint32_t mem[8]{};
    inline static unsigned reads = 0;
    inline static unsigned writes = 0;

    static std::uint32_t load(std::uintptr_t a) {
        ++reads;
        return mem[(a - kBase) / 4];
    }
    static void store(std::uintptr_t a, std::uint32_t v) {
        ++writes;
        mem[(a - kBase) / 4] = v;
    }
    static void reset() {
        for (auto& w : mem) w = 0;
        reads = 0;
        writes = 0;
    }
};

using Moder  = Register<Sim::kBase + 0, Access::ReadWrite, Sim>;   // GPIO mode register
using Status = Register<Sim::kBase + 4, Access::ReadOnly, Sim>;    // status flags
using Cmd    = Register<Sim::kBase + 8, Access::WriteOnly, Sim>;   // command register
using Cfg    = Register<Sim::kBase + 12, Access::ReadWrite, Sim>;  // configuration

using Pin5Mode  = Field<Moder, 10, 2, GpioMode>;
using Pin0Mode  = Field<Moder, 0, 2, GpioMode>;
using Ready     = Field<Status, 3, 1>;
using Enable    = Field<Cfg, 0, 1>;
using Prescaler = Field<Cfg, 4, 3>;  // raw 3-bit number
using TopBit    = Field<Cfg, 31, 1>;

// --- compile-time checks ------------------------------------------------------
static_assert(Pin5Mode::mask == 0xC00u, "pin 5 mask");
static_assert(Pin5Mode::offset == 10 && Pin5Mode::width == 2, "pin 5 geometry");
static_assert(Pin0Mode::mask == 0x3u, "pin 0 mask");
static_assert(Prescaler::mask == 0x70u, "prescaler mask");
static_assert(TopBit::mask == 0x80000000u, "bit 31 mask");
static_assert((Field<Cfg, 0, 32>::mask) == 0xFFFFFFFFu, "full-width field is legal");

// "Does F::write(V) compile?" Overload resolution failures are SFINAE-friendly.
template <typename F, typename V, typename = void>
struct can_write : std::false_type {};
template <typename F, typename V>
struct can_write<F, V, std::void_t<decltype(F::write(std::declval<V>()))>> : std::true_type {};

static_assert(can_write<Pin5Mode, GpioMode>::value, "enum value accepted");
static_assert(!can_write<Pin5Mode, int>::value, "raw int must NOT compile");
static_assert(!can_write<Pin5Mode, std::uint32_t>::value, "raw uint32 must NOT compile");

enum class OtherMode : std::uint32_t { A, B };
static_assert(!can_write<Pin5Mode, OtherMode>::value, "wrong enum must NOT compile");

// --- tests --------------------------------------------------------------------
TEST_CASE("whole-register read and write go through the storage policy") {
    Sim::reset();
    Moder::write(0xA5A5A5A5u);
    CHECK(Sim::mem[0] == 0xA5A5A5A5u);
    CHECK(Moder::read() == 0xA5A5A5A5u);
    Sim::mem[2] = 0;
    Cmd::write(7);  // write-only register: writing is allowed
    CHECK(Sim::mem[2] == 7u);
}

TEST_CASE("field write changes only its own bits") {
    Sim::reset();
    Sim::mem[0] = 0xFFFFFFFFu;
    Pin5Mode::write(GpioMode::Output);  // bits 11:10 become 01
    CHECK(Sim::mem[0] == 0xFFFFF7FFu);
    Pin5Mode::write(GpioMode::Input);   // bits 11:10 become 00
    CHECK(Sim::mem[0] == 0xFFFFF3FFu);
    Pin5Mode::write(GpioMode::Analog);  // bits 11:10 become 11
    CHECK(Sim::mem[0] == 0xFFFFFFFFu);
}

TEST_CASE("field write into an empty register lands at the right offset") {
    Sim::reset();
    Pin5Mode::write(GpioMode::AltFunc);  // 10b at bit 10
    CHECK(Sim::mem[0] == 0x800u);
    Pin0Mode::write(GpioMode::Output);   // 01b at bit 0
    CHECK(Sim::mem[0] == 0x801u);
}

TEST_CASE("field read returns the value shifted down") {
    Sim::reset();
    Sim::mem[0] = 0x800u;  // bits 11:10 = 10b
    CHECK((Pin5Mode::read() == GpioMode::AltFunc));
    CHECK((Pin0Mode::read() == GpioMode::Input));
    Sim::mem[1] = 0x8u;  // bit 3 of the read-only status register
    CHECK(Ready::read() == 1u);
    Sim::mem[1] = 0x0u;
    CHECK(Ready::read() == 0u);
}

TEST_CASE("set() and clear() touch a single bit") {
    Sim::reset();
    Sim::mem[3] = 0xF0F0F0F0u;
    Enable::set();
    CHECK(Sim::mem[3] == 0xF0F0F0F1u);
    Enable::clear();
    CHECK(Sim::mem[3] == 0xF0F0F0F0u);
    TopBit::clear();
    CHECK(Sim::mem[3] == 0x70F0F0F0u);
    TopBit::set();
    CHECK(Sim::mem[3] == 0xF0F0F0F0u);
}

TEST_CASE("a field write is exactly one read and one write") {
    Sim::reset();
    Pin5Mode::write(GpioMode::Output);
    CHECK(Sim::reads == 1u);
    CHECK(Sim::writes == 1u);
    Sim::reads = 0;
    Sim::writes = 0;
    Enable::set();
    CHECK(Sim::reads == 1u);
    CHECK(Sim::writes == 1u);
}

TEST_CASE("oversized raw values are masked and cannot spill into neighbours") {
    Sim::reset();
    Sim::mem[3] = 0x0u;
    Prescaler::write(0xFFu);  // only 3 bits fit
    CHECK(Sim::mem[3] == 0x70u);
    Sim::mem[3] = 0xFFFFFFFFu;
    Prescaler::write(0u);
    CHECK(Sim::mem[3] == 0xFFFFFF8Fu);
    Prescaler::write(5u);
    CHECK(Prescaler::read() == 5u);
}

TEST_CASE("write_const checks the value at compile time") {
    Sim::reset();
    Pin5Mode::write_const<GpioMode::Analog>();
    CHECK(Sim::mem[0] == 0xC00u);
    Prescaler::write_const<7u>();  // 7 fits in 3 bits; 8 would not compile
    CHECK(Prescaler::read() == 7u);
}
