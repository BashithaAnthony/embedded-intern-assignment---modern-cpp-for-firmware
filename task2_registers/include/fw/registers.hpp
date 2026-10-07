#pragma once
// Task 2: compile-time register and field access.
//   Register<Address, Access, Storage>      one 32-bit memory-mapped register
//   Field<Reg, Offset, Width, Value>        a bit field inside a register
// Address, offset, width and access rights are template parameters, so the
// compiler computes masks and rejects mistakes before the code ever runs.

#include <cstdint>
#include <type_traits>

namespace fw {

enum class Access { ReadOnly, WriteOnly, ReadWrite };

// ---------------------------------------------------------------------------
// Storage policies: how a register address is actually read and written.
// ---------------------------------------------------------------------------

// Real hardware: volatile access at a fixed address.
struct HwStorage {
    static inline std::uint32_t load(std::uintptr_t address) {
        return *reinterpret_cast<const volatile std::uint32_t*>(address);
    }
    static inline void store(std::uintptr_t address, std::uint32_t value) {
        *reinterpret_cast<volatile std::uint32_t*>(address) = value;
    }
};
// A policy only needs static load(address) and store(address, value).
// Tests supply their own policy backed by a plain array (see tests/test_task2.cpp).

// ---------------------------------------------------------------------------
// Register
// ---------------------------------------------------------------------------
template <std::uintptr_t Address, Access A, typename Storage = HwStorage>
struct Register {
    static constexpr std::uintptr_t address = Address;
    static constexpr Access access = A;
    using storage = Storage;

    [[nodiscard]] static std::uint32_t read() {
        static_assert(A != Access::WriteOnly, "Register::read: register is write-only");
        return Storage::load(Address);
    }

    static void write(std::uint32_t value) {
        static_assert(A != Access::ReadOnly, "Register::write: register is read-only");
        Storage::store(Address, value);
    }
};

// ---------------------------------------------------------------------------
// Field
// ---------------------------------------------------------------------------
// Value is the type callers use for the field: an enum class for multi-bit
// fields (so raw integers are rejected), or the default std::uint32_t.
template <typename Reg, unsigned Offset, unsigned Width, typename Value = std::uint32_t>
struct Field {
    static_assert(Width >= 1, "Field: width must be at least 1 bit");
    static_assert(Offset < 32, "Field: offset must be below 32");
    static_assert(Offset + Width <= 32, "Field: offset + width overflows a 32-bit register");
    static_assert(std::is_integral_v<Value> || std::is_enum_v<Value>,
                  "Field: Value must be an integer or an enum type");

    static constexpr unsigned offset = Offset;
    static constexpr unsigned width = Width;
    static constexpr std::uint32_t unshifted_mask = 0xFFFFFFFFu >> (32u - Width);
    static constexpr std::uint32_t mask = unshifted_mask << Offset;

    // Read the field, shifted down to bit 0.
    [[nodiscard]] static Value read() {
        static_assert(Reg::access != Access::WriteOnly, "Field::read: register is write-only");
        return static_cast<Value>((Reg::read() & mask) >> Offset);
    }

    // Read-modify-write: only this field changes. Bits of `v` that do not fit
    // in the field are discarded, so they can never spill into a neighbour.
    static void write(Value v) {
        static_assert(Reg::access != Access::ReadOnly, "Field::write: register is read-only");
        static_assert(Reg::access == Access::ReadWrite,
                      "Field::write needs a readable register (it is a read-modify-write)");
        const std::uint32_t shifted = (static_cast<std::uint32_t>(v) << Offset) & mask;
        Reg::write((Reg::read() & ~mask) | shifted);
    }

    // Same as write(), but the value is a template argument, so a value that
    // does not fit the field is a compile error instead of being masked.
    template <Value V>
    static void write_const() {
        static_assert((static_cast<std::uint32_t>(V) & ~unshifted_mask) == 0,
                      "Field::write_const: value does not fit in the field");
        write(V);
    }

    // One-bit fields only.
    static void set() {
        static_assert(Width == 1, "Field::set is only for 1-bit fields");
        static_assert(Reg::access != Access::ReadOnly, "Field::set: register is read-only");
        static_assert(Reg::access == Access::ReadWrite,
                      "Field::set needs a readable register (it is a read-modify-write)");
        Reg::write(Reg::read() | mask);
    }

    static void clear() {
        static_assert(Width == 1, "Field::clear is only for 1-bit fields");
        static_assert(Reg::access != Access::ReadOnly, "Field::clear: register is read-only");
        static_assert(Reg::access == Access::ReadWrite,
                      "Field::clear needs a readable register (it is a read-modify-write)");
        Reg::write(Reg::read() & ~mask);
    }
};

// ---------------------------------------------------------------------------
// Example field values
// ---------------------------------------------------------------------------
enum class GpioMode : std::uint32_t { Input = 0, Output = 1, AltFunc = 2, Analog = 3 };

}  // namespace fw
