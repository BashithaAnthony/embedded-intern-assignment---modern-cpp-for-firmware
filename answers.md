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

- A vtable is basically a hidden lookup table of function pointers that the compiler creates when you use virtual functions. When you call a virtual function, the program checks this table at runtime to figure out exactly which version of the function to run.

- For the overhead, it adds one vtable per class into your flash memory, which is just a list of pointers. For your RAM, it adds one hidden pointer (called a *vptr*, usually 4 bytes on a 32-bit microcontroller) to every single object you create so the object knows where its vtable is. 

### 2. Static (templates, CRTP) vs dynamic (virtual) polymorphism

Static polymorphism using templates is super fast because the compiler figures everything out in advance and can inline the code, meaning no runtime delays. However, it takes longer to compile and causes bigger code size (bloat) because the compiler generates a brand new copy of the function for every data type you use it with.

- Dynamic polymorphism (using virtual functions) gives you a lot of flexibility to swap out components at runtime—like changing which sensor is active—and keeps compile times and code size smaller. The downside is it is slightly slower at runtime because the processor has to look up the vtable every time a function is called.

### 3. Templates in headers; code bloat and how to limit it

- Templates aren't actually compiled code yet; they are just blueprints. When you try to use a template in a *.cpp* file, the compiler needs to see the full blueprint right then and there to generate the specific version of that code, which is why the whole definition has to sit in the header file.

- Code bloat happens when the compiler generates tons of almost identical, fully compiled copies of your template for all the different types you used, filling up your flash memory. You can limit it by moving the parts of the code that don't depend on the template parameters out into a regular, non-templated base class so that logic only gets compiled once.

### 4. `const` vs `constexpr` vs `consteval`; a case where only `constexpr` works

- *const* just means a variable cannot be modified once it is created, but its value might only be figured out at runtime.

- *constexpr* tells the compiler "if you know all the inputs right now, calculate this at compile time to save CPU cycles, but if the inputs only arrive at runtime, calculate it normally then".

- *consteval* is super strict—it must run at compile time, or the code will just throw an error and fail to build.

- *constexpr* is the only one that works for a math function that you want to use in two different ways: calculating a fixed delay during compile time using hardcoded numbers, but also using the exact same function to calculate a dynamic delay based on a live sensor reading at runtime.  

### 5. Why `enum class` for register field values

- A plain *enum* is basically just an integer in disguise. The compiler will let you accidentally compare an I2C speed enum with a completely unrelated UART parity enum, or even let you do math on them.

- An *enum class* locks the type down tightly. It forces you to use the exact type, meaning if a function asks for a *GpioMode*, you cannot accidentally pass a raw number or an ADC channel into it. This prevents a lot of silly hardware configuration bugs before the code even runs.  

## Errors, memory and concurrency

### 1. Error reporting without exceptions

- In firmware, we usually disable exceptions because they add hidden overhead. To report errors, return codes (like returning an *int* or *enum*) are the old-school C way, but they are risky because it's easy to accidentally ignore the return value and use garbage data.

- *std::optional* is great when a function might just fail to return a value (like trying to read a disconnected I2C sensor), but it doesn't tell you why it failed.

- *std::variant* lets you return either a good result type or an error type, but unpacking it can feel a bit clunky.

- *std::expected* (from C++23) is the best of both worlds: it holds either the expected return value or an error code, and the compiler basically forces you to handle the error path clearly. 

### 2. Cost of `std::function`; heap-free alternative

- *std::function* is super flexible for storing callbacks, but it has a hidden cost: if the lambda or function you're passing captures too many variables, it won't fit inside *std::function*'s small internal buffer. When that happens, it secretly calls *new* to allocate memory on the heap.

- In bare-metal firmware, hidden heap allocation is usually banned because it causes memory fragmentation. A heap-free alternative is to use a basic C-style function pointer along with a void* context pointer, or to write your own custom fixed-capacity wrapper that refuses to compile if the callback is too big.   

### 3. `reinterpret_cast<Packet*>(rx_buffer)`: aliasing, alignment, safe decoding

- No, it is definitely not safe. First, it breaks the "strict aliasing" rule, which means the compiler assumes pointers of different types don't point to the same memory. If you break this, the optimizer might accidentally delete your memory reads because it thinks they aren't connected.

- Second, there's alignment: a raw byte buffer might start at an odd memory address, but a struct with a 32-bit integer inside it needs to start at an address cleanly divisible by 4. If you cast it directly on an ARM Cortex chip like an STM32, you will trigger a hardware fault and crash the board.

- The safe way is to use *std::memcpy* (or *std::bit_cast* in C++20) to copy the raw bytes from the buffer directly into a properly aligned *Packet* struct.

### 4. `volatile` vs `std::atomic`; acquire/release guarantees

- *volatile* just tells the compiler, "hey, don't optimize out reads/writes to this variable because hardware (like an interrupt) might change it". But it does nothing to stop two RTOS threads from trying to read and write at the exact same millisecond, causing a data race, and it doesn't stop the CPU from reordering your instructions. *std::atomic* actually guarantees thread safety at the hardware level.

- *memory_order_acquire* ensures that any memory reads written after it in the code actually happen after it in reality.

- *memory_order_release* ensures that any memory writes written before it are completely finished and visible to other threads before the atomic variable gets updated.

### 5. Placement new and static memory pools

- Normally, *new* asks the operating system for a fresh chunk of heap memory. Placement *new* is a trick where you provide the memory address and say, "I already have this block of RAM, just run the constructor and build the object right here".

- In firmware, we use this to build static memory pools. We pre-allocate a big raw byte array globally at compile time so we never touch the heap. Then, when we need to spin up a new task or buffer at runtime, we use placement new to safely construct it right inside that pre-allocated array.
