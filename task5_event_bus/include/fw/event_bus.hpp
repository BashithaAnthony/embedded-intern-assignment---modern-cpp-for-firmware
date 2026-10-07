#pragma once
// Task 5: a thread-safe, fixed-capacity, allocation-free event bus.
//
//   * publish()      any thread, any time. Never waits for callbacks: it only holds a lock
//                    long enough to copy ONE event into a ring buffer, and it reports
//                    failure (returns false) when the queue is full.
//   * dispatch()     one consumer thread. Takes events out of the queue and calls the
//                    subscribed callbacks, with no lock held while a callback runs.
//   * subscribe() / unsubscribe()   any thread.
//
// All storage is inside the EventBus object: no heap, no std::function.
// Shared state is protected by two std::mutex objects that are never held together,
// plus one std::atomic flag that detects overlapping dispatch() calls.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <type_traits>

namespace fw::bus {

using EventId = std::uint16_t;

struct Event {
    EventId id{};
    std::uint32_t data{};
};

// A heap-free callable: plain function pointer + opaque context pointer.
struct Callback {
    using Function = void (*)(const Event&, void* context);

    Function function{nullptr};
    void* context{nullptr};

    bool valid() const { return function != nullptr; }
    void operator()(const Event& event) const { function(event, context); }
};

// Adapter: use a member function `void T::Method(const Event&)` as a callback.
//   Callback cb = bind_member<&Receiver::on_event>(receiver);
template <auto Method, typename T>
Callback bind_member(T& object) {
    return Callback{[](const Event& event, void* context) {
                        (static_cast<T*>(context)->*Method)(event);
                    },
                    &object};
}

// Identifies one subscription. The generation number makes a stale handle (one whose
// slot was reused by a later subscriber) harmless: unsubscribe() rejects it.
struct SubscriptionHandle {
    std::uint16_t slot{0xFFFF};
    std::uint16_t generation{0};

    bool valid() const { return slot != 0xFFFF; }
};

template <std::size_t MaxSubscribers, std::size_t QueueDepth>
class EventBus {
    static_assert(MaxSubscribers >= 1 && MaxSubscribers < 0xFFFF, "MaxSubscribers out of range");
    static_assert(QueueDepth >= 1, "QueueDepth must be at least 1");

public:
    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    // --- subscriptions ---------------------------------------------------------------
    // Returns an empty optional if the callback is null or all slots are in use.
    [[nodiscard]] std::optional<SubscriptionHandle> subscribe(EventId id, Callback callback) {
        if (!callback.valid()) {
            return std::nullopt;
        }
        std::lock_guard<std::mutex> lock(subs_mutex_);
        for (std::size_t i = 0; i < MaxSubscribers; ++i) {
            Slot& slot = slots_[i];
            if (!slot.active) {
                slot.callback = callback;
                slot.id = id;
                slot.active = true;
                ++slot.generation;
                return SubscriptionHandle{static_cast<std::uint16_t>(i), slot.generation};
            }
        }
        return std::nullopt;
    }

    // Returns true if the subscription existed. A callback that was already copied
    // into an in-progress dispatch() may still run once after this returns, so the
    // callback's context must stay valid until the next dispatch() has finished.
    bool unsubscribe(SubscriptionHandle handle) {
        if (!handle.valid() || handle.slot >= MaxSubscribers) {
            return false;
        }
        std::lock_guard<std::mutex> lock(subs_mutex_);
        Slot& slot = slots_[handle.slot];
        if (!slot.active || slot.generation != handle.generation) {
            return false;
        }
        slot.active = false;
        slot.callback = Callback{};
        return true;
    }

    // --- producers (any thread) ----------------------------------------------------------
    // Returns false (and drops the event) if the queue is full. Never waits for a consumer.
    [[nodiscard]] bool publish(const Event& event) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (count_ == QueueDepth) {
            ++dropped_;
            return false;
        }
        queue_[(head_ + count_) % QueueDepth] = event;
        ++count_;
        ++published_;
        return true;
    }

    // --- consumer (one thread) -----------------------------------------------------------
    // Delivers the events that are queued when the call starts (events published
    // meanwhile, including by callbacks, wait for the next call, so one call always ends).
    // Returns the number of events taken from the queue. If another dispatch() is already
    // running (another thread, or a callback calling dispatch()), returns 0 immediately.
    std::size_t dispatch() {
        DispatchGuard guard(dispatching_);
        if (!guard.owns()) {
            return 0;
        }
        std::size_t budget = pending();
        std::size_t delivered = 0;
        Event event;
        while (budget > 0 && pop(event)) {
            --budget;
            deliver(event);
            ++delivered;
        }
        return delivered;
    }

    // --- statistics ----------------------------------------------------------------------
    std::size_t pending() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return count_;
    }
    std::size_t published() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return published_;
    }
    std::size_t dropped() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return dropped_;
    }

private:
    struct Slot {
        Callback callback{};
        EventId id{};
        std::uint16_t generation{0};
        bool active{false};
    };

    // Sets the "dispatching" flag for its lifetime, unless somebody else already holds it.
    class DispatchGuard {
    public:
        explicit DispatchGuard(std::atomic<bool>& flag)
            : flag_{flag}, owns_{!flag.exchange(true, std::memory_order_acquire)} {}
        ~DispatchGuard() {
            if (owns_) {
                flag_.store(false, std::memory_order_release);
            }
        }
        DispatchGuard(const DispatchGuard&) = delete;
        DispatchGuard& operator=(const DispatchGuard&) = delete;
        bool owns() const { return owns_; }

    private:
        std::atomic<bool>& flag_;
        bool owns_;
    };

    bool pop(Event& out) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (count_ == 0) {
            return false;
        }
        out = queue_[head_];
        head_ = (head_ + 1) % QueueDepth;
        --count_;
        return true;
    }

    // Copy the matching callbacks out under the lock, then call them with no lock held.
    // That keeps callbacks free to publish, subscribe and unsubscribe without deadlock.
    void deliver(const Event& event) {
        Callback matching[MaxSubscribers];
        std::size_t n = 0;
        {
            std::lock_guard<std::mutex> lock(subs_mutex_);
            for (const Slot& slot : slots_) {
                if (slot.active && slot.id == event.id) {
                    matching[n++] = slot.callback;
                }
            }
        }
        for (std::size_t i = 0; i < n; ++i) {
            matching[i](event);
        }
    }

    // queue state: guarded by queue_mutex_
    mutable std::mutex queue_mutex_;
    Event queue_[QueueDepth]{};
    std::size_t head_{0};
    std::size_t count_{0};
    std::size_t published_{0};
    std::size_t dropped_{0};

    // subscriber table: guarded by subs_mutex_
    mutable std::mutex subs_mutex_;
    Slot slots_[MaxSubscribers]{};

    // detects overlapping dispatch() calls
    std::atomic<bool> dispatching_{false};
};

}  // namespace fw::bus
