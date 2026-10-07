# Report

## 1. Design decisions (per task)
TODO

## 2. Task 1: Compiler Explorer comparison (raw int vs Quantity, -O2)
TODO (link + assembly summary)

## 3. Task 2: Compiler Explorer comparison (Field::write vs hand-written mask/shift)
TODO

## 4. Task 3: test coverage of Figure 1 arrows and ignored events
TODO

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
