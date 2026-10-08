#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <thread>

#include "fw/event_bus.hpp"

using namespace fw::bus;

constexpr EventId kA = 1;
constexpr EventId kB = 2;
constexpr EventId kC = 3;

struct Counter {
    unsigned count = 0;
    std::uint32_t last = 0;
};
void on_count(const Event& e, void* context) {
    auto* c = static_cast<Counter*>(context);
    ++c->count;
    c->last = e.data;
}

struct Log {
    std::uint32_t values[32]{};
    unsigned n = 0;
};
void on_log(const Event& e, void* context) {
    auto* log = static_cast<Log*>(context);
    log->values[log->n++] = e.data;
}

TEST_CASE("events reach only the subscribers of that event id") {
    EventBus<4, 8> bus;
    Counter a, b;
    CHECK(bus.subscribe(kA, Callback{on_count, &a}).has_value());
    CHECK(bus.subscribe(kB, Callback{on_count, &b}).has_value());

    CHECK(bus.publish(Event{kA, 10}));
    CHECK(bus.publish(Event{kA, 11}));
    CHECK(bus.publish(Event{kB, 20}));
    CHECK(bus.publish(Event{kC, 30}));  // nobody subscribed to kC
    CHECK(bus.dispatch() == 4u);

    CHECK(a.count == 2u);
    CHECK(a.last == 11u);
    CHECK(b.count == 1u);
}

TEST_CASE("events arrive in order, also after the ring buffer wraps around") {
    EventBus<4, 4> bus;  // only 4 slots
    Log log;
    CHECK(bus.subscribe(kA, Callback{on_log, &log}).has_value());

    std::uint32_t next = 0;
    for (int round = 0; round < 5; ++round) {  // 15 events through 4 slots
        for (int i = 0; i < 3; ++i) CHECK(bus.publish(Event{kA, next++}));
        bus.dispatch();
    }

    CHECK(log.n == 15u);
    bool in_order = true;
    for (unsigned i = 0; i < log.n; ++i) {
        if (log.values[i] != i) in_order = false;
    }
    CHECK(in_order);
}

TEST_CASE("publish returns false when the queue is full") {
    EventBus<2, 3> bus;
    Counter c;
    CHECK(bus.subscribe(kA, Callback{on_count, &c}).has_value());

    for (std::uint32_t i = 0; i < 3; ++i) CHECK(bus.publish(Event{kA, i}));
    CHECK(!bus.publish(Event{kA, 99}));  // full: the answer comes back at once

    CHECK(bus.dispatch() == 3u);         // the dropped event is gone
    CHECK(c.count == 3u);
    CHECK(bus.publish(Event{kA, 4}));    // there is room again
}

TEST_CASE("unsubscribe stops delivery") {
    EventBus<2, 4> bus;
    Counter c;
    const auto handle = bus.subscribe(kA, Callback{on_count, &c});
    CHECK(handle.has_value());
    if (!handle) return;

    CHECK(bus.publish(Event{kA, 1}));
    bus.dispatch();
    CHECK(c.count == 1u);

    CHECK(bus.unsubscribe(*handle));
    CHECK(!bus.unsubscribe(*handle));  // already removed
    CHECK(bus.publish(Event{kA, 2}));
    bus.dispatch();
    CHECK(c.count == 1u);              // not delivered any more
}

TEST_CASE("subscribe fails when every slot is used, or the callback is null") {
    EventBus<2, 4> bus;
    Counter c;
    CHECK(bus.subscribe(kA, Callback{on_count, &c}).has_value());
    CHECK(bus.subscribe(kB, Callback{on_count, &c}).has_value());
    CHECK(!bus.subscribe(kC, Callback{on_count, &c}).has_value());  // full
    CHECK(!bus.subscribe(kC, Callback{}).has_value());              // null callback
}

// --- stress test: 4 producer threads, 1 consumer thread -----------------------------
namespace {

constexpr unsigned kProducers = 4;
constexpr std::uint32_t kPerProducer = 100000;
constexpr std::size_t kTotal = kProducers * kPerProducer;
constexpr EventId kStress = 7;

// How many times each (producer, sequence number) was delivered.
// Only the consumer thread writes it.
std::uint8_t g_seen[kProducers][kPerProducer];

void on_stress(const Event& e, void* context) {
    ++g_seen[e.data >> 24][e.data & 0xFFFFFFu];  // data = (producer << 24) | sequence
    ++*static_cast<std::size_t*>(context);       // total delivered
}

}  // namespace

TEST_CASE("stress: 4 producers x 100000 events, every event delivered exactly once") {
    EventBus<4, 1024> bus;
    std::size_t delivered = 0;
    CHECK(bus.subscribe(kStress, Callback{on_stress, &delivered}).has_value());

    std::thread consumer([&] {
        while (delivered < kTotal) {
            if (bus.dispatch() == 0) std::this_thread::yield();
        }
    });

    std::thread producers[kProducers];
    for (unsigned p = 0; p < kProducers; ++p) {
        producers[p] = std::thread([&bus, p] {
            for (std::uint32_t seq = 0; seq < kPerProducer; ++seq) {
                const Event event{kStress, (p << 24) | seq};
                while (!bus.publish(event)) std::this_thread::yield();  // queue full: try again
            }
        });
    }

    for (auto& t : producers) t.join();
    consumer.join();

    std::size_t not_exactly_once = 0;
    for (const auto& row : g_seen) {
        for (const std::uint8_t times : row) {
            if (times != 1) ++not_exactly_once;
        }
    }
    CHECK(delivered == kTotal);
    CHECK(not_exactly_once == 0u);
}
