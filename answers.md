# Part A: Knowledge check


## Object lifetime and resources

### 1. Rule of zero / three / five

- Rule of Zero: If your class doesn't directly manage raw resources (like raw pointers or open hardware handles), you shouldn't write custom destructors, copy, or move constructors. Let the compiler generate the default ones.

- Rule of Three: If your class manages a raw resource and needs a custom destructor to clean it up, you also need to write a custom copy constructor and copy assignment operator to prevent double-free errors.

- Rule of Five: If you need the Rule of Three, you should also write a move constructor and move assignment operator so your class can be transferred efficiently without expensive copying.

- Most classes should follow the Rule of Zero. It keeps the code much cleaner and pushes resource management down into standard containers or smart wrappers, which drastically reduces the chances of accidentally leaking memory when handling complex data like sensor arrays.

### 2. What does `std::move` actually do? Moved-from states

- std::move doesn't actually move anything on its own. It just performs a cast to an rvalue reference (&&), telling the compiler, "I am done with this object, feel free to steal its resources."

- A moved-from *std::unique_ptr* is left completely empty, holding a *nullptr*.

- A moved-from custom user type is left in a "valid but unspecified state." This means it is safe to destroy it or assign a new value to it, but you shouldn't try to read its data because you don't know what's left inside.

### 3. Why `noexcept` move constructors? Standard-library example

- You mark move constructors with *noexcept* to promise the compiler that moving the object won't throw an error and crash the program halfway through.

- If a move constructor is not marked *noexcept*, *std::vector* will refuse to use it when resizing its internal buffer. Instead, *std::vector* will fall back to copying every single element to the new memory block just to be safe, which wastes a lot of CPU cycles.

### 4. When must a base class have a virtual destructor?

- A base class must have a virtual destructor if you ever plan to delete a derived class object through a base class pointer.

- If you don't make it virtual, the compiler only runs the base class's destructor. The derived class's destructor gets completely ignored. If that derived class was holding onto something specific—like an open UART port or an SPI buffer for a display—that resource will leak.

### 5. Static initialisation order problem and two fixes

- The "Static Initialization Order Fiasco" happens when you have global variables in different source files, and one relies on the other. C++ doesn't guarantee which one initializes first. If your system tries to start a motor controller global before the GPIO pin global is ready, the board crashes before *main()* even starts.

- Fix 1: Use *constexpr* initialization so the values are baked in at compile time, completely bypassing the runtime initialization order.

- Fix 2: Use the "Construct On First Use" idiom. Instead of a global variable, wrap it in a function that contains a *static* local variable and returns a reference to it. It will only initialize the exact moment you call it the first time.

## Polymorphism and templates

### 1. How a vtable works; RAM/flash cost per class and per object
TODO

### 2. Static (templates, CRTP) vs dynamic (virtual) polymorphism
TODO

### 3. Templates in headers; code bloat and how to limit it
TODO

### 4. `const` vs `constexpr` vs `consteval`; a case where only `constexpr` works
TODO

### 5. Why `enum class` for register field values
TODO

## Errors, memory and concurrency

### 1. Error reporting without exceptions
TODO

### 2. Cost of `std::function`; heap-free alternative
TODO

### 3. `reinterpret_cast<Packet*>(rx_buffer)`: aliasing, alignment, safe decoding
TODO

### 4. `volatile` vs `std::atomic`; acquire/release guarantees
TODO

### 5. Placement new and static memory pools
TODO
