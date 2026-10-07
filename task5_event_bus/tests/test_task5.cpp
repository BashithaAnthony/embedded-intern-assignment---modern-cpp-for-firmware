#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <thread>
#include <type_traits>

#include "fw/event_bus.hpp"

using namespace fw::bus;

// --- detect ThreadSanitizer (GCC and Clang spell it differently) --------------------------------
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define FW_TSAN 1
#endif
#endif
#if defined(__SANITIZE_THREAD__) && !defined(FW_TSAN)
#define FW_TSAN 1
#endif

// --- allocation counter: lets a test PROVE the bus never touches the heap ------------------------
// Not built under ThreadSanitizer: Clang's TSan runtime already defines operator new/delete,
// so replacing them here would be a duplicate definition at link time.
#ifndef FW_TSAN
static std::atomic<std::size_t> g_alloc_count{0};

void* operator new(std::size_t size) {
    g_alloc_count.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(size)) return p;
    std::abort();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
#endif

// --- helpers ------------------------------------------------------------------------------
constexpr EventId kA = 1;
constexpr EventId kB = 2;
constexpr EventId kC = 3;

using SmallBus = EventBus<4, 4>;

struct Counter {
    unsigned count = 0;
    std::uint32_t last = 0;
};
void on_count(const Event& e, void* ctx) {
    auto* c = static_cast<Counter*>(ctx);
    ++c->count;
    c->last = e.data;
}

struct Log {
    std::uint32_t values[64]{};
    unsigned n = 0;
};
void on_log(const Event& e, void* ctx) {
    auto* l = static_cast<Log*>(ctx);
    if (l->n < 64) l->values[l->n++] = e.data;
}

struct Receiver {
    unsigned sum = 0;
    void on_event(const Event& e) { sum += e.data; }
};

static_assert(std::is_trivially_copyable_v<Event>, "events are copied under a lock: keep them trivial");
static_assert(std::is_trivially_copyable_v<Callback>, "callbacks are plain pointers");
static_assert(!std::is_copy_constructible_v<SmallBus>, "a bus is not copyable");

// --- single-thread behaviour --------------------------------------------------------------
TEST_CASE("events reach only the subscribers of that event id") {
    SmallBus bus;
    Counter a, b;
    CHECK(bus.subscribe(kA, Callback{on_count, &a}).has_value());
    CHECK(bus.subscribe(kB, Callback{on_count, &b}).has_value());

    CHECK(bus.publish(Event{kA, 10}));
    CHECK(bus.publish(Event{kA, 11}));
    CHECK(bus.publish(Event{kB, 20}));
    CHECK(bus.publish(Event{kC, 30}));  // nobody subscribed: consumed and dropped silently
    CHECK(bus.dispatch() == 4u);

    CHECK(a.count == 2u);
    CHECK(a.last == 11u);
    CHECK(b.count == 1u);
    CHECK(b.last == 20u);
    CHECK(bus.pending() == 0u);
}

TEST_CASE("several subscribers of one id are all called") {
    SmallBus bus;
    Counter x, y;
    CHECK(bus.subscribe(kA, Callback{on_count, &x}).has_value());
    CHECK(bus.subscribe(kA, Callback{on_count, &y}).has_value());
    CHECK(bus.publish(Event{kA, 5}));
    bus.dispatch();
    CHECK(x.count == 1u);
    CHECK(y.count == 1u);
}

TEST_CASE("events are delivered in FIFO order, across ring-buffer wrap-around") {
    SmallBus bus;  // queue depth 4
    Log log;
    CHECK(bus.subscribe(kA, Callback{on_log, &log}).has_value());

    std::uint32_t next = 0;
    for (int round = 0; round < 10; ++round) {  // 30 events through a 4-slot queue
        for (int i = 0; i < 3; ++i) CHECK(bus.publish(Event{kA, next++}));
        CHECK(bus.dispatch() == 3u);
    }
    CHECK(log.n == 30u);
    bool in_order = true;
    for (unsigned i = 0; i < log.n; ++i) {
        if (log.values[i] != i) in_order = false;
    }
    CHECK(in_order);
}

TEST_CASE("publish reports failure when the queue is full, and never blocks") {
    SmallBus bus;  // depth 4
    Counter c;
    CHECK(bus.subscribe(kA, Callback{on_count, &c}).has_value());

    for (std::uint32_t i = 0; i < 4; ++i) CHECK(bus.publish(Event{kA, i}));
    CHECK(!bus.publish(Event{kA, 99}));  // returns immediately with false
    CHECK(!bus.publish(Event{kA, 100}));
    CHECK(bus.pending() == 4u);
    CHECK(bus.published() == 4u);
    CHECK(bus.dropped() == 2u);

    CHECK(bus.dispatch() == 4u);  // the dropped events are gone, the first 4 survive
    CHECK(c.count == 4u);
    CHECK(c.last == 3u);
    CHECK(bus.publish(Event{kA, 7}));  // space is available again
}

TEST_CASE("dispatch on an empty queue does nothing") {
    SmallBus bus;
    CHECK(bus.dispatch() == 0u);
}

TEST_CASE("unsubscribe stops delivery; handles are checked") {
    SmallBus bus;
    Counter c;
    const auto h = bus.subscribe(kA, Callback{on_count, &c});
    CHECK(h.has_value());
    if (!h) return;

    CHECK(bus.publish(Event{kA, 1}));
    bus.dispatch();
    CHECK(c.count == 1u);

    CHECK(bus.unsubscribe(*h));
    CHECK(!bus.unsubscribe(*h));  // already gone
    CHECK(bus.publish(Event{kA, 2}));
    bus.dispatch();
    CHECK(c.count == 1u);  // no longer delivered

    CHECK(!bus.unsubscribe(SubscriptionHandle{}));        // invalid handle
    CHECK(!bus.unsubscribe(SubscriptionHandle{200, 1}));  // slot out of range
}

TEST_CASE("a stale handle cannot remove a later subscriber that reused its slot") {
    EventBus<1, 4> bus;  // a single slot, so reuse is guaranteed
    Counter first, second;
    const auto h1 = bus.subscribe(kA, Callback{on_count, &first});
    CHECK(h1.has_value());
    if (!h1) return;
    CHECK(bus.unsubscribe(*h1));

    const auto h2 = bus.subscribe(kA, Callback{on_count, &second});
    CHECK(h2.has_value());
    if (!h2) return;

    CHECK(!bus.unsubscribe(*h1));  // stale: same slot, old generation
    CHECK(bus.publish(Event{kA, 1}));
    bus.dispatch();
    CHECK(second.count == 1u);  // the new subscriber is still there
    CHECK(first.count == 0u);
}

TEST_CASE("subscribe fails when full or when the callback is null; slots can be reused") {
    EventBus<2, 4> bus;
    Counter c;
    const auto h1 = bus.subscribe(kA, Callback{on_count, &c});
    const auto h2 = bus.subscribe(kB, Callback{on_count, &c});
    CHECK(h1.has_value());
    CHECK(h2.has_value());
    CHECK(!bus.subscribe(kC, Callback{on_count, &c}).has_value());  // full
    CHECK(!bus.subscribe(kC, Callback{}).has_value());              // null callback

    if (!h1) return;
    CHECK(bus.unsubscribe(*h1));
    CHECK(bus.subscribe(kC, Callback{on_count, &c}).has_value());  // a slot is free again
}

TEST_CASE("bind_member turns a member function into a callback") {
    SmallBus bus;
    Receiver r;
    CHECK(bus.subscribe(kA, bind_member<&Receiver::on_event>(r)).has_value());
    CHECK(bus.publish(Event{kA, 5}));
    CHECK(bus.publish(Event{kA, 7}));
    bus.dispatch();
    CHECK(r.sum == 12u);
}

// --- re-entrancy: callbacks may use the bus --------------------------------------------------
struct ReentrantCtx {
    SmallBus* bus = nullptr;
    unsigned calls = 0;
    std::size_t nested_dispatch_result = 99;
    SubscriptionHandle own{};
    bool unsubscribed = false;
};

void on_publish_follow_up(const Event& e, void* ctx) {  // publishes a kB event
    auto* c = static_cast<ReentrantCtx*>(ctx);
    ++c->calls;
    (void)c->bus->publish(Event{kB, e.data + 1});
}
void on_nested_dispatch(const Event&, void* ctx) {  // calls dispatch() from a callback
    auto* c = static_cast<ReentrantCtx*>(ctx);
    c->nested_dispatch_result = c->bus->dispatch();
}
void on_unsubscribe_self(const Event&, void* ctx) {
    auto* c = static_cast<ReentrantCtx*>(ctx);
    ++c->calls;
    c->unsubscribed = c->bus->unsubscribe(c->own);
}

TEST_CASE("a callback may publish; the follow-up event waits for the next dispatch") {
    SmallBus bus;
    ReentrantCtx ctx;
    ctx.bus = &bus;
    Counter b;
    CHECK(bus.subscribe(kA, Callback{on_publish_follow_up, &ctx}).has_value());
    CHECK(bus.subscribe(kB, Callback{on_count, &b}).has_value());

    CHECK(bus.publish(Event{kA, 41}));
    CHECK(bus.dispatch() == 1u);  // only the event that was queued when the call began
    CHECK(ctx.calls == 1u);
    CHECK(b.count == 0u);
    CHECK(bus.pending() == 1u);

    CHECK(bus.dispatch() == 1u);
    CHECK(b.count == 1u);
    CHECK(b.last == 42u);
}

TEST_CASE("dispatch() called from inside a callback returns 0 instead of recursing") {
    SmallBus bus;
    ReentrantCtx ctx;
    ctx.bus = &bus;
    CHECK(bus.subscribe(kA, Callback{on_nested_dispatch, &ctx}).has_value());
    CHECK(bus.publish(Event{kA, 1}));
    CHECK(bus.dispatch() == 1u);
    CHECK(ctx.nested_dispatch_result == 0u);
}

TEST_CASE("a callback may unsubscribe itself without deadlock") {
    SmallBus bus;
    ReentrantCtx ctx;
    ctx.bus = &bus;
    const auto h = bus.subscribe(kA, Callback{on_unsubscribe_self, &ctx});
    CHECK(h.has_value());
    if (!h) return;
    ctx.own = *h;

    CHECK(bus.publish(Event{kA, 1}));
    CHECK(bus.publish(Event{kA, 2}));
    bus.dispatch();
    CHECK(ctx.unsubscribed);
    CHECK(ctx.calls == 1u);  // the second event found no subscriber
}

#ifndef FW_TSAN  // see the note on the allocation counter above
TEST_CASE("subscribe, publish, dispatch and unsubscribe never allocate") {
    SmallBus bus;
    Counter c;
    const std::size_t before = g_alloc_count.load();
    const auto h = bus.subscribe(kA, Callback{on_count, &c});
    const bool ok = bus.publish(Event{kA, 1});
    const std::size_t n = bus.dispatch();
    const bool removed = h.has_value() && bus.unsubscribe(*h);
    const std::size_t after = g_alloc_count.load();

    CHECK(ok);
    CHECK(n == 1u);
    CHECK(removed);
    CHECK(after == before);
}
#endif

// --- multi-threaded -----------------------------------------------------------------------------
namespace stress {

constexpr unsigned kProducers = 4;
constexpr std::uint32_t kPerProducer = 100000;
constexpr std::uint64_t kTotal = static_cast<std::uint64_t>(kProducers) * kPerProducer;
constexpr EventId kEvent = 7;

using Bus = EventBus<8, 1024>;

std::uint8_t g_seen[kProducers][kPerProducer];  // static storage, written only by the consumer

struct Ctx {
    std::uint64_t delivered = 0;
    std::uint64_t duplicates = 0;
    std::uint64_t invalid = 0;
};

void on_event(const Event& e, void* raw) {
    auto* c = static_cast<Ctx*>(raw);
    const std::uint32_t producer = e.data >> 24;
    const std::uint32_t seq = e.data & 0xFFFFFFu;
    if (producer >= kProducers || seq >= kPerProducer) {
        ++c->invalid;
        return;
    }
    if (g_seen[producer][seq]++ != 0) ++c->duplicates;
    ++c->delivered;
}

}  // namespace stress

TEST_CASE("stress: 4 producers x 100000 events, one consumer, every event delivered exactly once") {
    using namespace stress;
    for (auto& row : g_seen)
        for (auto& cell : row) cell = 0;

    Bus bus;
    Ctx ctx;
    CHECK(bus.subscribe(kEvent, Callback{on_event, &ctx}).has_value());

    std::atomic<bool> go{false};
    std::uint64_t failed_attempts[kProducers] = {};
    bool timed_out = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);

    std::thread consumer([&] {
        while (ctx.delivered + ctx.invalid < kTotal) {
            if (bus.dispatch() == 0) {
                if (std::chrono::steady_clock::now() > deadline) {
                    timed_out = true;
                    break;
                }
                std::this_thread::yield();
            }
        }
    });

    std::thread producers[kProducers];
    for (unsigned p = 0; p < kProducers; ++p) {
        producers[p] = std::thread([&, p] {
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            std::uint64_t failed = 0;
            for (std::uint32_t seq = 0; seq < kPerProducer; ++seq) {
                const Event ev{kEvent, (p << 24) | seq};
                while (!bus.publish(ev)) {  // queue full: publish said so, retry later
                    ++failed;
                    std::this_thread::yield();
                }
            }
            failed_attempts[p] = failed;
        });
    }

    go.store(true, std::memory_order_release);
    for (auto& t : producers) t.join();
    consumer.join();

    std::uint64_t missing = 0;
    for (unsigned p = 0; p < kProducers; ++p)
        for (std::uint32_t s = 0; s < kPerProducer; ++s)
            if (g_seen[p][s] != 1) ++missing;
    std::uint64_t failed_total = 0;
    for (const std::uint64_t f : failed_attempts) failed_total += f;

    CHECK(!timed_out);
    CHECK(ctx.delivered == kTotal);
    CHECK(ctx.duplicates == 0u);
    CHECK(ctx.invalid == 0u);
    CHECK(missing == 0u);
    CHECK(bus.published() == kTotal);
    CHECK(bus.dropped() == failed_total);  // every full-queue rejection was reported and counted
    CHECK(bus.pending() == 0u);
}

namespace churn {
std::atomic<std::uint64_t> g_hits{0};
void on_hit(const Event&, void*) { g_hits.fetch_add(1, std::memory_order_relaxed); }
}  // namespace churn

TEST_CASE("subscribe/unsubscribe churn races safely with publish and dispatch") {
    using namespace churn;
    EventBus<4, 256> bus;
    g_hits = 0;
    std::atomic<bool> done{false};
    std::atomic<std::uint64_t> sent{0};

    std::thread churner([&] {
        for (int i = 0; i < 5000; ++i) {
            const auto h = bus.subscribe(kA, Callback{on_hit, nullptr});
            if (h) (void)bus.unsubscribe(*h);
        }
        done.store(true, std::memory_order_release);
    });

    std::thread producer([&] {
        std::uint32_t n = 0;
        while (!done.load(std::memory_order_acquire)) {
            if (bus.publish(Event{kA, n++})) sent.fetch_add(1, std::memory_order_relaxed);
            else std::this_thread::yield();
        }
    });

    while (!done.load(std::memory_order_acquire)) {
        if (bus.dispatch() == 0) std::this_thread::yield();
    }
    churner.join();
    producer.join();
    bus.dispatch();

    CHECK(g_hits.load() <= sent.load());  // nothing was delivered that was not sent
}