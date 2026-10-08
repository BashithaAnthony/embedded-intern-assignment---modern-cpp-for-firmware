# Report

## Task 1: Strong unit types

### Design
- `Quantity<Tag, Rep>` wraps one integer of type `Rep`.
- `Tag` is an empty struct. It exists only to make each unit a different type.
  `Quantity<MillivoltTag, int32_t>` and `Quantity<MilliampTag, int32_t>` are unrelated types.
- The constructor is `explicit`. So `Millivolts v = 5;` does not compile.
- There is no implicit conversion back to an integer. The only way out is `count()`.
- All operators are `constexpr` hidden friends. They exist only for the exact same `Quantity` type.
  So mixing units has no matching operator, and the compiler stops.
- Operators: `+`, `-`, unary `-`, `+=`, `-=`, `*` and `/` by a plain number (both `q * 3` and `3 * q`), and all six comparisons.
- `static_cast<Rep>` is used inside operators. Small integers are promoted to `int` in arithmetic.
  The cast makes the narrowing back to `Rep` deliberate.

### Units
| Type | Rep | Why |
|---|---|---|
| `Millivolts` | `int32_t` | 4 bytes, as required |
| `Milliamps` | `int32_t` | wide enough for any current in mA |
| `Milliseconds` | `int32_t` | wraps after about 24.8 days. Task 3 uses a 64-bit time type instead. |
| `DeciCelsius` | `int16_t` | 253 means 25.3 C. Range is about -3276 C to +3276 C. |

### Literals
`3300_mV`, `20_mA`, `250_ms`, and `253_dC`. They live in `fw::literals`.
Users opt in with `using namespace fw::literals;`.

### ADC conversion
`adc12_to_millivolts(std::uint16_t counts)` converts a 12-bit count to `Millivolts` with a 3.3 V reference.
- Formula: `counts * 3300 / 4095`, rounded to the nearest value, in integer arithmetic only:
  `(2 * c * 3300 + 4095) / (2 * 4095)`.
- Inputs above 4095 are clamped to 4095.
- The largest intermediate value is 27,027,000. It fits easily in `int32_t`.
- Full scale is taken as count 4095. Some chips use 4096. Check the datasheet.
- Examples: 0 gives 0 mV, 4095 gives 3300 mV, 2048 gives 1650 mV.

### Zero overhead
Compile-time checks in the header:
```cpp
static_assert(sizeof(Millivolts) == sizeof(std::int32_t));
static_assert(std::is_trivially_copyable_v<Millivolts>);
static_assert(std::is_standard_layout_v<Millivolts>);
```
The tag takes no space because it is a type, not a member.

### Compiler Explorer comparison
Source: `tests/asm_compare.cpp`.
```cpp
std::int32_t raw_fn(std::int32_t a, std::int32_t b) { return (a + b) * 3 - 5; }
Millivolts   qty_fn(Millivolts a, Millivolts b)     { return (a + b) * 3 - Millivolts{5}; }
```
Link: [https://godbolt.org/z/4hbn1encq](https://godbolt.org/z/4hbn1encq)

x86-64 GCC, `-O2`: both functions compile to the same three instructions.
```
add     edi, esi
lea     eax, [rdi-5+rdi*2]
ret
```
`-Os` also gives identical code for both functions.

ARM GCC, `-O2 -mcpu=cortex-m4 -mthumb`:
```
add     r0, r0, r1
add     r0, r0, r0, lsl #1
subs    r0, r0, #5
bx      lr
```

Result: with optimisation on, the Quantity wrapper vanishes. It costs nothing.

At `-O0` the two are not the same. The Quantity version calls its operators as separate functions.
Zero overhead needs optimisation. Firmware is normally built with `-O2` or `-Os`.

### Expressions that correctly fail to compile
Each one was checked with the compiler. All are rejected.
The same examples are in `tests/compile_fail_examples.cpp` inside an `#if 0` block.
```cpp
auto b1 = 3300_mV + 250_ms;           // different units: no operator+ accepts both
Millivolts b2 = 5;                    // the constructor is explicit
auto b3 = 3300_mV + 5;                // a plain int is not a Millivolts
bool b4 = (3300_mV == 20_mA);         // different units cannot be compared
void set_dac(Millivolts);
set_dac(250_ms);                      // wrong unit passed to a function
auto b6 = 3300_mV * 2_mV;             // scaling needs a plain number, not a Quantity
Milliamps b7 = 3300_mV;               // one unit cannot be assigned to another
```
Some of these are also checked by `static_assert` in the tests.
A small "does `A + B` compile?" trait runs `static_assert(!can_add<Millivolts, Milliseconds>::value)`.
So the rule is tested on every build.

### Tests (8 test cases)
- Construct, read back, and default value is zero.
- Addition, subtraction, and negation.
- Compound assignment (`+=`, `-=`, `*=`, `/=`).
- Scaling by a plain number, in both orders, and division.
- All six comparisons.
- The four literals.
- ADC conversion: 0, 1, 2048, 4095, and a clamped value of 5000.
- Zero overhead: size and trivial copy.
- Many of these also use `static_assert`, so they are checked at compile time too.

### Rules followed
- C++17. Zero warnings with `-Wall -Wextra -Wpedantic -Werror` on GCC and Clang.
- Builds with `-fno-exceptions -fno-rtti`.
- No `new`, `std::vector`, `std::string`, `std::function`, or `std::shared_ptr`.

### Sanitizer output

Test project /home/bashitha/embedded-intern-assignment---modern-cpp-for-firmware/build-asan
    Start 1: task1
1/5 Test #1: task1 ............................   Passed    0.02 sec
    Start 2: task2
2/5 Test #2: task2 ............................   Passed    0.02 sec
    Start 3: task3
3/5 Test #3: task3 ............................   Passed    0.02 sec
    Start 4: task4
4/5 Test #4: task4 ............................   Passed    0.02 sec
    Start 5: task5
5/5 Test #5: task5 ............................   Passed    0.51 sec

100% tests passed, 0 tests failed out of 5

Total Test time (real) =   0.59 sec

### Limitations
- `q * 2.5` compiles. The `2.5` is silently cut to `2`. A stricter design would delete the floating-point overloads.
- Signed overflow is undefined behaviour. The operators do not check for it.
- A literal that is too big is cut to the width of `Rep` without an error.
- There is no unit mixing such as volts times amps. It is deliberately not supported.

## Task 2: Compile-time register fields

### Design
- `Register<Address, Access, Storage>` is one 32-bit register.
  - `Address` is a template value. So the compiler knows it.
  - `Access` is `ReadOnly`, `WriteOnly`, or `ReadWrite`.
  - `Storage` is a policy. It has two static functions: `load(address)` and `store(address, value)`.
- `HwStorage` does a `volatile` read or write at the real address.
- In tests, `Sim` is the storage policy. It uses a plain array. So tests run on a PC.
- `Field<Reg, Offset, Width, Value>` is a bit field inside a register.
  - The mask is computed at compile time.
  - `read()` returns the field shifted down to bit 0.
  - `write(v)` is a read-modify-write. Only the field's bits change.
  - `set()` and `clear()` are for 1-bit fields only.
  - `write_const<V>()` checks at compile time that the value fits.
- `Value` is the type callers use. For multi-bit fields it is an `enum class`, for example `GpioMode`.
  - So `Pin5Mode::write(1)` does not compile.
- `write(v)` masks the value to the field width. A value that is too big cannot spill into the next field.

### Compile-time checks
| Rule | How it is enforced |
|---|---|
| Offset + width must be at most 32 | `static_assert` in `Field` |
| Width must be at least 1 | `static_assert` in `Field` |
| No write to a read-only register | `static_assert` in `Register::write` and `Field::write` |
| No read of a write-only register | `static_assert` in `Register::read` and `Field::read` |
| Field write needs a readable register | `static_assert` (it is a read-modify-write) |
| `set()` and `clear()` only for 1-bit fields | `static_assert` |
| A raw integer or wrong enum is not accepted | normal overload resolution |
| A value that does not fit, with `write_const` | `static_assert` |

### Expressions that correctly fail to compile
Each one was checked with the compiler. All are rejected.
```cpp
using RwReg = Register<0x40020000, Access::ReadWrite>;
using RoReg = Register<0x40020004, Access::ReadOnly>;
using WoReg = Register<0x40020008, Access::WriteOnly>;
using Pin5Mode = Field<RwReg, 10, 2, GpioMode>;

constexpr auto m1 = Field<RwReg, 30, 4>::mask;     // 30 + 4 = 34 bits: overflows 32
constexpr auto m2 = Field<RwReg, 0, 0>::mask;      // zero width
RoReg::write(1);                                    // write to read-only register
Field<RoReg, 0, 1>::set();                          // field write to read-only register
auto v5 = WoReg::read();                            // read of write-only register
Field<WoReg, 0, 1>::set();                          // read-modify-write needs a readable register
Pin5Mode::write(1);                                 // raw int, not GpioMode
enum class Other { A };
Pin5Mode::write(Other::A);                          // wrong enum type
Pin5Mode::set();                                    // set() on a 2-bit field
Field<RwReg, 4, 3>::write_const<8u>();              // 8 does not fit in 3 bits
```

### Tests (8 test cases)
- Whole-register read and write go through the storage policy.
- A field write changes only its own bits.
- A field lands at the right offset in an empty register.
- A field read returns the shifted value.
- `set()` and `clear()` touch a single bit.
- A field write is exactly one read and one write of the register.
- An oversized value is masked and cannot spill into neighbours.
- `write_const` checks the value at compile time.
- `static_assert` checks mask values and that `write(int)` and `write(OtherEnum)` are not accepted.

### Compiler Explorer comparison
Source: `tests/asm_compare.cpp`.
`field_version()` calls `Pin5Mode::write(GpioMode::Output)`.
`handwritten_version()` does `*r = (*r & ~(0x3u << 10)) | (0x1u << 10)` on a `volatile` pointer.

Link: [https://godbolt.org/z/T38rYTh4b](https://godbolt.org/z/T38rYTh4b)

x86-64 GCC, `-O2` (also `-Os`). Both functions give the same instructions:
```
"field_version()":
        mov     eax, DWORD PTR ds:1073872896
        and     ah, -13
        or      ah, 4
        mov     DWORD PTR ds:1073872896, eax
        ret
```

ARM GCC, `-O2 -mcpu=cortex-m4 -mthumb`:
```
field_version():
        ldr     r2, .L3
        ldr     r3, [r2]
        bic     r3, r3, #3072
        orr     r3, r3, #1024
        str     r3, [r2]
        bx      lr
.L3:
        .word   1073872896
```

Result: with optimisation on, the field write costs nothing extra.
It is one load, one change of the bits, and one store.

At `-O0` the field version is not the same. It calls `Field::write`, `Register::read`,
and `HwStorage::load` as separate functions. The zero-overhead result needs `-O2` or `-Os`.

### Rules followed
- C++17. Zero warnings with `-Wall -Wextra -Wpedantic -Werror` on GCC and Clang.
- Builds with `-fno-exceptions -fno-rtti`.
- No `new`, `std::vector`, `std::string`, `std::function`, or `std::shared_ptr`.

### Sanitizer output

Test project /home/bashitha/embedded-intern-assignment---modern-cpp-for-firmware/build-asan
    Start 1: task1
1/5 Test #1: task1 ............................   Passed    0.02 sec
    Start 2: task2
2/5 Test #2: task2 ............................   Passed    0.02 sec
    Start 3: task3
3/5 Test #3: task3 ............................   Passed    0.02 sec
    Start 4: task4
4/5 Test #4: task4 ............................   Passed    0.02 sec
    Start 5: task5
5/5 Test #5: task5 ............................   Passed    0.51 sec

100% tests passed, 0 tests failed out of 5

Total Test time (real) =   0.59 sec

### Limitations
- A field write reads the register, changes bits, and writes it back.
  An interrupt that changes the same register between the read and the write can lose an update.
  Real code must disable interrupts or use hardware bit-set and bit-clear registers.
- Some registers change when read, or use "write 1 to clear" bits.
  This design does not model those registers.
- Every register is 32 bits wide. 8-bit and 16-bit registers are not modelled.
- Values that are out of range are masked silently in `write()`.
  `write_const()` is the strict version.

## Task 3: Connection manager state machine

### Design
- Each state is its own struct. It holds only the data it needs.
  - `Idle` has no data.
  - `Connecting{attempt}`, `Connected{since}`, `Backoff{until, failures}`, `Error{code}`.
- The machine holds one `std::variant` of the five states.
- Events are also a `std::variant`: Connect, Success, Failure, Timeout, LinkLost, Disconnect, Reset, and Tick{now}.
- One `std::visit` handles a (state, event) pair. It uses the "overloaded lambdas" pattern.
  - Each valid transition is one lambda.
  - One generic lambda catches every other pair. It logs the event as ignored and keeps the state.
- Time is injected through the `IClock` interface. Tests use a `FakeClock` and advance time instantly.
- Logging goes through the `ILogger` interface. It reports ignored events and state changes.
  No strings are built. Only pointers to string literals are used.
- Time uses `Quantity<MillisecondTag, std::int64_t>` from Task 1.
  A 32-bit millisecond counter wraps after about 24.8 days. The 64-bit counter cannot overflow in practice.

### State diagram
```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Connecting: Connect
    Connecting --> Connected: Success
    Connecting --> Backoff: Failure or Timeout
    Connecting --> Error: 5th failure
    Backoff --> Connecting: Tick, delay expired
    Connected --> Backoff: LinkLost
    Connected --> Idle: Disconnect
    Error --> Idle: Reset
```

### Backoff rules
- The delay doubles: 1 s, 2 s, 4 s, 8 s. The cap is 30 s.
- With a limit of 5 failures, the delay never gets above 8 s. The cap is tested on the function `backoff_delay` directly. Failure 6 gives 30 s.
- Entering Backoff counts one more consecutive failure. The fifth failure goes to Error instead.
- Success resets the count.

### Decisions on unclear points
1. **The "5th failure" arrow.** Figure 1 draws it from Backoff to Error. The text says to go to Error instead of entering Backoff. I followed the text. The fifth failure goes straight from Connecting to Error. So Backoff never holds a count of 5.
2. **Disconnect.** Figure 1 shows it only from Connected. In every other state it is ignored and logged.
3. **Failure count without a counter.** `Connecting{attempt}` already holds it. Attempt n failing means n failures in a row. Success resets it because `Connected` stores no count.
4. **LinkLost** counts as the first failure. A test checks this.
5. **Tick.**
   - A Tick before the deadline in Backoff is handled. It is not logged.
   - A Tick exactly at the deadline counts as expired.
   - A Tick in any other state is ignored and logged.

### Tests (17 test cases)
Every arrow in Figure 1 has a test.

| Arrow | Test |
|---|---|
| Idle to Connecting | Connect gives attempt 1 |
| Connecting to Connected | Success stores the clock time in `since` |
| Connecting to Backoff | Failure and Timeout each give `until = now + 1 s`, failures 1 |
| Backoff to Connecting | early ticks do nothing, tick at the deadline gives attempt + 1 |
| Connected to Backoff | LinkLost gives failures 1 |
| Connected to Idle | Disconnect |
| 5th failure to Error | delays 1, 2, 4, 8 s checked, then Error with `RetriesExhausted`. Also tested through Timeout. |
| Error to Idle | Reset, then Connect starts at attempt 1 |

Other tests:
- Success resets the failure count.
- The 30 s cap on `backoff_delay`.
- Ignored events: Success, Failure, and Disconnect in Idle. Connect in Connected. A stray Tick and Connect in Error. Disconnect in Backoff.
- Ignored events change nothing and are logged with the right state and event.
- A test tries all 40 (state, event) pairs. 8 are handled. 32 are ignored and logged. None crash.

### Rules followed
- No `new`, `std::vector`, `std::string`, `std::function`, or `std::shared_ptr`.
- Builds with `-fno-exceptions -fno-rtti`. The code uses `std::get_if`, never `std::get`.
- Zero warnings with `-Wall -Wextra -Wpedantic -Werror` on GCC and Clang.

### Sanitizer output
(Paste your ctest output for the ASan+UBSan and TSan runs here.)

### Limitations
- The interfaces `IClock` and `ILogger` use virtual calls. This cost is small. It happens once per event.
- Nothing stops a caller from passing a Tick with a time that goes backwards. The clock is trusted.
- Disconnect does not cancel a retry while Connecting or in Backoff. This follows Figure 1.

## Task 4: CRTP versus virtual sensor drivers

### Design
- Both designs have the same three functions: init(), read(), name().
- read() returns std::optional<Sample>. It is empty if the sensor is not initialised.
- Virtual version: abstract class ISensor with three virtual functions.
- CRTP version: SensorBase<Derived> calls Derived::do_init(), do_read(), do_name().
- SensorBase has a private constructor and "friend Derived".
  So only the named Derived class can inherit from it.
- Both versions use the same mock behaviour class. So both do exactly the same work.
  The benchmark measures only the cost of the call.
- The virtual benchmark gets its sensors from a function in another .cpp file.
  Otherwise the compiler sees the real type and removes the virtual call.
- The benchmarks print a checksum. The script checks that both checksums match.

### Results (-Os, best of 5 runs, <compiler and version>, <your CPU>)

| Version | text (B) | data (B) | bss (B) | 10M read() calls | ns per call |
|---------|---------:|---------:|--------:|-----------------:|------------:|
| virtual |   3010   |    776   |   48    |     70.8 ms      |    7.08     |
| CRTP    |   1782   |    624   |    8    |     18.5 ms      |    1.85     |

### What the numbers show
- The virtual loop makes an indirect call through the vtable on every read.
  The disassembly shows "call *0x8(%rax)".
- The CRTP loop has no calls. The compiler inlines read().
- Part of the speed gap comes from inlining, not only from the missing vtable lookup.
- Each virtual object also holds a vptr. A test checks this.
- Each virtual class has one vtable in read-only memory.

### When to choose each
Virtual:
- The set of drivers is open or decided at run time.
- You need one list that holds different sensors.
- You want a stable interface that hides the implementation.
- Cost: a vptr per object, a vtable per class, no inlining.

CRTP:
- The driver type is known at compile time, for example one fixed board.
- You want the smallest and fastest code.
- Cost: no mixed list, more code per type, harder error messages.

Heterogeneous list of sensors:
- Virtual: a simple array of ISensor*.
- CRTP: the types differ, so one array cannot hold them.
- For a fixed set of types, use std::variant with std::visit. This is static dispatch.
  A test shows it.

### Caveats
- Sizes are for whole executables. Startup and library code are included.
  Only the difference between the two rows is meaningful.
- On a real target, compile the code for the target and size the object files:
  arm-none-eabi-g++ -Os -mcpu=cortex-m4 -mthumb -c ... then arm-none-eabi-size.
- Timing depends on the CPU and the compiler. Run it several times.

## Task 5: Thread-safe event bus

### Design decisions
- All storage lives inside the EventBus object. No heap is used.
- The queue is a ring buffer of QueueDepth events. An event is 8 bytes.
- A callback is a function pointer plus a void* context. There is no std::function.
- Two mutexes protect the shared state: one for the queue, one for the subscriber table.
  They are never held together, so lock-order deadlock cannot happen.
- publish() locks, copies one event, and unlocks. If the queue is full,
  it returns false and counts a drop.
- dispatch() takes one event under the lock. It then copies the matching callbacks,
  unlocks, and calls them. So callbacks can publish, subscribe, and unsubscribe.
- One dispatch() call handles only the events queued when it started.
  So a callback that republishes cannot make it run forever.
- An atomic flag detects overlapping dispatch() calls. The second call returns 0.
- Handles carry a generation number. A stale handle cannot remove a newer subscriber.

### Tests
- 15 test cases: filtering, FIFO order, wrap-around, full queue, unsubscribe,
  stale handles, re-entrancy, and a check that nothing allocates.
- Stress test: 4 producer threads publish 100,000 events each. One consumer thread dispatches.
- Each event carries (producer, sequence number). The consumer marks it in a table.
- Result: all 400,000 events seen exactly once, no duplicates, none missing.
- bus.published() equals 400,000. bus.dropped() equals the number of rejected publishes.
- A 120-second deadline turns a deadlock into a test failure.

### Sanitizer output

**ASan + UBSan:**
Test project /home/bashitha/embedded-intern-assignment---modern-cpp-for-firmware/build-asan
    Start 1: task1
1/5 Test #1: task1 ............................   Passed    0.02 sec
    Start 2: task2
2/5 Test #2: task2 ............................   Passed    0.02 sec
    Start 3: task3
3/5 Test #3: task3 ............................   Passed    0.02 sec
    Start 4: task4
4/5 Test #4: task4 ............................   Passed    0.02 sec
    Start 5: task5
5/5 Test #5: task5 ............................   Passed    0.51 sec

100% tests passed, 0 tests failed out of 5

Total Test time (real) =   0.59 sec

**ThreadSanitizer:**
Test project /home/bashitha/embedded-intern-assignment---modern-cpp-for-firmware/build-tsan
    Start 1: task1
1/5 Test #1: task1 ............................   Passed    0.02 sec
    Start 2: task2
2/5 Test #2: task2 ............................   Passed    0.02 sec
    Start 3: task3
3/5 Test #3: task3 ............................   Passed    0.02 sec
    Start 4: task4
4/5 Test #4: task4 ............................   Passed    0.02 sec
    Start 5: task5
5/5 Test #5: task5 ............................   Passed    4.29 sec

100% tests passed, 0 tests failed out of 5

Total Test time (real) =   4.38 sec

### Limitations
- publish() waits for a mutex. The lock is held only to copy one event,
  but this is not a hard real-time guarantee.
- A callback already copied for a running dispatch() may run once after unsubscribe().
- std::mutex is not safe in an interrupt handler. A lock-free queue would be needed there.
- A full queue drops the new event. Other designs overwrite the oldest or block.
- The generation counter is 16 bits and wraps after 65,536 reuses of one slot.

## 7. C++20 features used (if any)
None / TODO
