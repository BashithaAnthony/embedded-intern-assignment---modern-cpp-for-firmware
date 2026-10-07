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
TODO

### 2. Cost of `std::function`; heap-free alternative
TODO

### 3. `reinterpret_cast<Packet*>(rx_buffer)`: aliasing, alignment, safe decoding
TODO

### 4. `volatile` vs `std::atomic`; acquire/release guarantees
TODO

### 5. Placement new and static memory pools
TODO
