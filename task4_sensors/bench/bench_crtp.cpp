// Benchmark: 10 million read() calls on concrete CRTP sensors (static dispatch).
#include <chrono>
#include <cstdint>
#include <cstdio>

#include "fw/sensor_crtp.hpp"

int main() {
    constexpr std::uint32_t kCalls = 10'000'000;

    fw::CrtpTemperatureSensor a;
    fw::CrtpPressureSensor b;
    a.init();
    b.init();

    std::int64_t checksum = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::uint32_t i = 0; i < kCalls / 2; ++i) {
        const auto sa = a.read();
        const auto sb = b.read();
        if (sa) checksum += sa->value;
        if (sb) checksum += sb->value;
    }
    const auto t1 = std::chrono::steady_clock::now();

    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::printf("RESULT crtp calls=%u time_us=%lld checksum=%lld\n", kCalls,
                static_cast<long long>(us), static_cast<long long>(checksum));
    return 0;
}
