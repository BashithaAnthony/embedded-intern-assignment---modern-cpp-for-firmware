#pragma once
// Task 2: compile-time register and field access.
//   Register<Address, Access, Storage>   one 32-bit memory-mapped register
//   Field<Reg, Offset, Width, Value>     a bit field inside a register
// Address, offset, width and access rights are template parameters, so the compiler
// computes the masks and rejects mistakes before the code ever runs.

#include <cstdint>

namespace fw {

enum class Access { ReadOnly, WriteOnly, ReadWrite };

// Storage policy for real hardware: volatile access at a fixed address.
// A policy only needs static load(address) and store(address, value).
// The tests use their own policy backed by a plain array.
struct HwStorage {
    static std::uint32_t load(std::uintptr_t address) {
        return *reinterpret_cast<const volatile std::uint32_t*>(address);
    }
    static void store(std::uintptr_t address, std::uint32_t value) {
        *reinterpret_cast<volatile std::uint32_t*>(address) = value;
    }
};

template <std::uintptr_t Address, Access A, typename Storage = HwStorage>
struct Register {
    static std::uint32_t read() {
        static_assert(A != Access::WriteOnly, "register is write-only");
        return Storage::load(Address);
    }
    static void write(std::uint32_t value) {
        static_assert(A != Access::ReadOnly, "register is read-only");
        Storage::store(Address, value);
    }
};

// Value is the type callers use for the field: an enum class for multi-bit fields
// (so a raw integer does not compile), or the default std::uint32_t.
template <typename Reg, unsigned Offset, unsigned Width, typename Value = std::uint32_t>
struct Field {
    static_assert(Width >= 1, "field width must be at least 1");
    static_assert(Offset + Width <= 32, "field overflows a 32-bit register");

    static constexpr std::uint32_t mask = (0xFFFFFFFFu >> (32u - Width)) << Offset;

    // The field value, shifted down to bit 0.
    static Value read() { return static_cast<Value>((Reg::read() & mask) >> Offset); }

    // Read-modify-write: only this field changes. Bits of `v` that do not fit
    // in the field are cut off, so they cannot spill into a neighbouring field.
    static void write(Value v) {
        const std::uint32_t bits = (static_cast<std::uint32_t>(v) << Offset) & mask;
        Reg::write((Reg::read() & ~mask) | bits);
    }

    // One-bit fields only.
    static void set() {
        static_assert(Width == 1, "set() is only for 1-bit fields");
        Reg::write(Reg::read() | mask);
    }
    static void clear() {
        static_assert(Width == 1, "clear() is only for 1-bit fields");
        Reg::write(Reg::read() & ~mask);
    }
};

// Example value type for a 2-bit field.
enum class GpioMode : std::uint32_t { Input, Output, AltFunc, Analog };

}  // namespace fw
