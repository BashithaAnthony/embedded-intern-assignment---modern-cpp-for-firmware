// Benchmark: 10 million read() calls through ISensor* (virtual dispatch).
#include <chrono>
#include <cstdint>
#include <cstdio>

#include "fw/sensor_virtual.hpp"

namespace fw {
ISensor& bench_sensor(unsigned index);  // defined in virtual_factory.cpp
}

int main() {
    constexpr std::uint32_t kCalls = 10'000'000;

    fw::ISensor* a = &fw::bench_sensor(0);
    fw::ISensor* b = &fw::bench_sensor(1);
    a->init();
    b->init();

    std::int64_t checksum = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::uint32_t i = 0; i < kCalls / 2; ++i) {
        const auto sa = a->read();
        const auto sb = b->read();
        if (sa) checksum += sa->value;
        if (sb) checksum += sb->value;
    }
    const auto t1 = std::chrono::steady_clock::now();

    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::printf("RESULT virtual calls=%u time_us=%lld checksum=%lld\n", kCalls,
                static_cast<long long>(us), static_cast<long long>(checksum));
    return 0;
}
