# Part A: Knowledge check

## Object lifetime and resources

### 1. Rule of zero / three / five

- **Rule of Zero:** If my class does not own a raw resource (raw pointer, open handle), I write no destructor, copy or move. The compiler's defaults are enough.
- **Rule of Three:** If I need a custom destructor, I also need a custom copy constructor and copy assignment. Otherwise two objects can hold the same pointer and both free it (double free).
- **Rule of Five:** Rule of Three plus a move constructor and move assignment, so the object can be moved cheaply. If I write a destructor or copy, the compiler does not make the moves, so moves silently become copies.
- Most classes should follow Rule of Zero. Members like `std::array` and `std::unique_ptr` clean up by themselves, so fewer leaks.

```cpp
struct SensorLog {
    std::array<uint16_t, 64> samples{};   // Rule of Zero, nothing else needed
};
```

### 2. What does `std::move` actually do? Moved-from states

- `std::move` does not move anything. It only casts to an rvalue reference (`&&`) to say "I am done with this object". The move constructor / assignment does the real moving.
- A moved-from `std::unique_ptr` is guaranteed to be `nullptr`.
- A moved-from standard type (like `std::string`) is "valid but unspecified". I can destroy it or assign to it, but should not rely on what is inside.
- In my own move constructor I must reset the source (e.g. set a pointer to `nullptr`), because raw pointers are just copied.

```cpp
auto a = std::make_unique<int>(5);
auto b = std::move(a);   // a is nullptr now
```

### 3. Why `noexcept` move constructors? Standard-library example

- `noexcept` promises the function will not throw.
- When `std::vector` grows, it moves the old elements to the new buffer. If a move could throw halfway, the old data would be damaged. So `std::vector` only moves if the move constructor is `noexcept`.
- If not, it copies every element instead, which wastes time and memory.

```cpp
Buffer(Buffer&& other) noexcept;   // vector will move
```

### 4. When must a base class have a virtual destructor?

- When I delete a derived object through a base class pointer.
- Without `virtual` this is undefined behaviour. Usually only the base destructor runs, so something the derived class holds (like an open SPI bus) is never released.

```cpp
struct Display { virtual ~Display() = default; };
struct SpiDisplay : Display { ~SpiDisplay() override { /* release SPI */ } };
Display* d = new SpiDisplay;
delete d;   // calls ~SpiDisplay too, only because ~Display is virtual
```

### 5. Static initialisation order problem and two fixes

- Global objects in different `.cpp` files are initialised in an unknown order. A global `MotorController` may use a global `Gpio` that is not constructed yet. This happens before `main()`, so it is hard to debug.
- **Fix 1:** Use `constexpr` / `constinit` so the value is set at compile time.
- **Fix 2:** "Construct on first use". Make the object a `static` local inside a function. It is created the first time the function is called.

```cpp
Gpio& gpio() {
    static Gpio g{5};   // created on first call
    return g;
}
```

## Polymorphism and templates

### 1. How a vtable works; RAM/flash cost per class and per object

- A vtable is a table of function pointers. There is one per class that has virtual functions. Each object has a hidden `vptr` pointing to its class's vtable. A virtual call reads the `vptr`, then the function pointer, then jumps to it.
- **Flash (per class):** one vtable, about 4 bytes per virtual function on a 32-bit MCU, plus an RTTI entry.
- **RAM (per object):** one `vptr`, 4 bytes on a 32-bit MCU, no matter how many virtual functions.

```cpp
struct A { int x; };                    // 4 bytes
struct B { int x; virtual void f(); };  // 8 bytes on 32-bit (vptr + x)
```

### 2. Static (templates, CRTP) vs dynamic (virtual) polymorphism

- **Static:** the type is known at compile time, so the call can be inlined. No vtable, no runtime cost. But compile time is longer and each type gets its own copy of the code (bigger flash).
- **Dynamic:** the type is chosen at runtime, so I can swap objects (like the active sensor). Code is smaller, but there is a `vptr` in RAM and a small cost per call.
- CRTP means the base is a template that takes the derived class as its parameter, so the derived function is found at compile time.

```cpp
template <typename D>
struct SensorBase { int read() { return static_cast<D*>(this)->readImpl(); } };
struct TempSensor : SensorBase<TempSensor> { int readImpl() { return 25; } };
```

### 3. Templates in headers; code bloat and how to limit it

- A template is only a blueprint. To make the real code for `Ring<int, 8>`, the compiler needs the full definition at that point, so it goes in the header.
- Code bloat: every different type makes a new full copy of the code, so flash fills up.
- To limit it, move the code that does not depend on `T` into a normal non-template base class, so it is compiled once. Removing unused code with `--gc-sections` also helps.

```cpp
class RingBase { /* head, tail, index logic: compiled once */ };

template <typename T, std::size_t N>
class Ring : private RingBase { T data_[N]; };   // only this part is copied per type
```

### 4. `const` vs `constexpr` vs `consteval`; a case where only `constexpr` works

- `const`: cannot be changed after creation, but the value may be decided at runtime.
- `constexpr`: a variable must be known at compile time. A function can run at compile time if the inputs are known, and runs normally at runtime otherwise.
- `consteval`: the function must run at compile time, or it is a compile error.
- Only `constexpr` works when one function is needed for both cases: a fixed delay at compile time and a delay from a sensor value at runtime.

```cpp
constexpr uint32_t ticks(uint32_t ms) { return ms * 48000; }

constexpr uint32_t kBoot = ticks(10);   // compile time
uint32_t d = ticks(sensorMs);           // runtime (consteval would fail here)
```

### 5. Why `enum class` for register field values

- A plain `enum` converts to `int` automatically, so I can pass a raw number or mix unrelated enums (I2C speed and UART parity) without an error. Its names also leak into the surrounding scope.
- An `enum class` is its own type. No automatic conversion, and I must write `Parity::Even`. Wrong enums or raw numbers give a compile error.
- I can also set the size, like `uint8_t`, to match the register field.

```cpp
enum class Parity : uint8_t { None, Even, Odd };
void setParity(Parity p);

setParity(Parity::Even);   // ok
// setParity(1);           // error
```

## Errors, memory and concurrency

### 1. Error reporting without exceptions

- Exceptions are usually off in firmware (`-fno-exceptions`) because they add flash size and unpredictable timing.
- **Return codes:** simple, but the caller can ignore them and use garbage data. `[[nodiscard]]` helps.
- **`std::optional`:** good when "no value" is the only failure, but it does not say why.
- **`std::variant`:** can hold a value or an error, but unpacking it is clunky.
- **`std::expected` (C++23):** holds the value or an error code, so the caller has to unwrap it and sees the error path. Best option if the compiler supports it.

```cpp
std::expected<uint16_t, Err> readTemp() {
    if (!ready) return std::unexpected(Err::Timeout);
    return 250;
}
```

### 2. Cost of `std::function`; heap-free alternative

- `std::function` has a small internal buffer. If a lambda captures too much, it calls `new` on the heap. In firmware that causes fragmentation and unpredictable timing.
- It also adds an indirect call and can throw `std::bad_function_call`.
- Heap-free alternative: a plain function pointer plus a `void*` context. Another option is my own fixed-size wrapper with a `static_assert` on the size.

```cpp
using Callback = void (*)(void* ctx);
struct Button { Callback cb; void* ctx; };
```

### 3. `reinterpret_cast<Packet*>(rx_buffer)`: aliasing, alignment, safe decoding

- No, it is not safe.
- **Aliasing:** reading a byte buffer through a `Packet*` breaks the strict aliasing rule (undefined behaviour). The optimizer may reorder or remove reads.
- **Alignment:** the buffer may start at any address, but a `Packet` with a `uint32_t` needs an address divisible by 4. On Cortex-M0 this gives a HardFault. On M3/M4 it often works but is not safe to rely on.
- Also padding and byte order (endianness) may not match the sender.
- Safe way: copy the bytes into a `Packet` with `std::memcpy` (or `std::bit_cast` in C++20). The compiler makes this fast. Check the length first.

```cpp
Packet p;
std::memcpy(&p, rx_buffer, sizeof p);   // safe copy
```

### 4. `volatile` vs `std::atomic`; acquire/release guarantees

- `volatile` only stops the compiler from removing or merging reads/writes. It is for hardware registers. It does not make operations atomic and does not stop reordering, so it cannot fix data races.
- `std::atomic` makes operations atomic and lets me choose the memory ordering. It is the right tool to share data between threads or an ISR and main code.
- **Release (store):** all writes before it are done and visible before the store.
- **Acquire (load):** nothing after it can move before it.
- They work as a pair: if the acquire load sees the value from a release store, it also sees all the writes before that store.

```cpp
data = 42;
ready.store(true, std::memory_order_release);      // producer

if (ready.load(std::memory_order_acquire)) use(data);   // consumer sees 42
```

### 5. Placement new and static memory pools

- Normal `new` takes memory from the heap and then runs the constructor. Placement `new` skips the allocation. I give it an address, and it only runs the constructor there.
- In firmware, I make a big byte array at compile time (no heap) and build objects inside it when needed. This is a static memory pool.
- The array must be aligned (`alignas`), and I must call the destructor myself because there is no `delete`.

```cpp
alignas(Packet) static unsigned char pool[sizeof(Packet) * 4];

Packet* p = new (&pool[0]) Packet{};   // build inside the pool
p->~Packet();                          // destroy manually
```
