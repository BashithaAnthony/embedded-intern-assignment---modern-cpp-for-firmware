# Modern C++ for Firmware

Embedded intern assignment: a set of small, heap-free, header-only C++17 libraries that show how modern C++ makes firmware safer without making it slower.

| Task | Topic | Key ideas |
|------|-------|-----------|
| 1 | [Strong unit types](task1_units) | `Quantity<Tag, Rep>`, `constexpr` arithmetic, user-defined literals, zero overhead |
| 2 | [Compile-time register fields](task2_registers) | `Register<Addr, Access>`, `Field<Reg, Offset, Width>`, `enum class`, storage policy |
| 3 | [Connection state machine](task3_connection_fsm) | `std::variant`, `std::visit`, overloaded lambdas, injected clock |
| 4 | [CRTP vs virtual sensors](task4_sensors) | static vs dynamic polymorphism, size and speed benchmark |
| 5 | [Thread-safe event bus](task5_event_bus) | fixed capacity, mutex/atomics, ThreadSanitizer |

Written answers live in [`answers.md`](answers.md) (Part A) and the write-up in [`report.md`](report.md).

## Requirements

- CMake >= 3.20
- GCC and Clang with C++17 support (the code must build warning-free on both)
- Git and internet access on first configure (doctest is fetched with CMake `FetchContent`)

## Build and test

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Sanitizers

```bash
# AddressSanitizer + UndefinedBehaviorSanitizer
cmake -S . -B build-asan -DSANITIZE=address,undefined
cmake --build build-asan -j && ctest --test-dir build-asan --output-on-failure

# ThreadSanitizer (needed for Task 5)
cmake -S . -B build-tsan -DSANITIZE=thread
cmake --build build-tsan -j && ctest --test-dir build-tsan --output-on-failure
```

### Everything at once

`scripts/check_all.sh` builds and tests GCC and Clang, each plain, ASan+UBSan and TSan. Run it from a fresh clone before submitting.

## Project rules enforced by the build

- `-std=c++17 -Wall -Wextra -Wpedantic -Werror`, zero warnings on GCC and Clang
- Tasks 1-4 also build with `-fno-exceptions -fno-rtti`
- Tasks 1-4 use no `new`, `std::vector`, `std::string`, `std::function` or `std::shared_ptr`
- Task 5 uses no `std::function` (function pointer + `void*` context, or a custom fixed-size callable)

## Layout

```
.
├── CMakeLists.txt
├── cmake/                  # warnings, sanitizers, doctest
├── scripts/check_all.sh    # full compiler x sanitizer matrix
├── task1_units/            # include/fw/units.hpp, tests/
├── task2_registers/
├── task3_connection_fsm/
├── task4_sensors/
├── task5_event_bus/
├── answers.md              # Part A
└── report.md               # design notes, Compiler Explorer results, table, sanitizer output
```

Each task folder has the same shape: `include/fw/<header>.hpp` (the library) and `tests/test_taskN.cpp` (doctest, at least 6 test cases).

## Progress

- [x] Task 1: strong unit types
- [x] Task 2: register fields
- [x] Task 3: connection state machine
- [x] Task 4: CRTP vs virtual (with benchmark table)
- [x] Task 5: event bus (TSan clean)
- [x] Part A answers
- [x] Report and sanitizer output
- [ ] Clean-clone build verified
