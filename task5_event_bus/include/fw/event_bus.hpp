#pragma once
// Task 5: a thread-safe, fixed-capacity, allocation-free event bus.
//
//   publish()      any thread. Holds a lock only long enough to copy ONE event into a
//                  ring buffer. Returns false if the queue is full. Never waits for callbacks.
//   dispatch()     ONE consumer thread. Takes events out of the queue and calls the callbacks.
//   subscribe() / unsubscribe()   any thread.
//
// All storage is inside the EventBus object: no heap, no std::function.
// Two mutexes protect the shared state: one for the queue, one for the subscriber table.
// They are never held together, so they cannot deadlock each other.

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

namespace fw::bus {

using EventId = std::uint16_t;

struct Event {
    EventId id{};
    std::uint32_t data{};
};

// A heap-free callable: a plain function pointer plus a context pointer.
struct Callback {
    void (*function)(const Event&, void* context) = nullptr;
    void* context = nullptr;
};

using Handle = std::size_t;  // index of the subscriber slot

template <std::size_t MaxSubscribers, std::size_t QueueDepth>
class EventBus {
public:
    // Returns an empty optional if the callback is null or every slot is in use.
    std::optional<Handle> subscribe(EventId id, Callback callback) {
        if (callback.function == nullptr) {
            return std::nullopt;
        }
        std::lock_guard<std::mutex> lock(subs_mutex_);
        for (std::size_t i = 0; i < MaxSubscribers; ++i) {
            if (!slots_[i].active) {
                slots_[i] = Slot{callback, id, true};
                return i;
            }
        }
        return std::nullopt;
    }

    // After this returns, the callback is never called again.
    bool unsubscribe(Handle handle) {
        std::lock_guard<std::mutex> lock(subs_mutex_);
        if (handle >= MaxSubscribers || !slots_[handle].active) {
            return false;
        }
        slots_[handle].active = false;
        return true;
    }

    // Returns false (and drops the event) if the queue is full.
    [[nodiscard]] bool publish(const Event& event) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (count_ == QueueDepth) {
            return false;
        }
        queue_[(head_ + count_) % QueueDepth] = event;
        ++count_;
        return true;
    }

    // Delivers every queued event, in order. Returns how many there were.
    // Call it from ONE thread only. A callback may call publish(),
    // but must not call subscribe() or unsubscribe() (the subscriber lock is held).
    std::size_t dispatch() {
        std::size_t delivered = 0;
        Event event;
        while (pop(event)) {
            deliver(event);
            ++delivered;
        }
        return delivered;
    }

private:
    struct Slot {
        Callback callback;
        EventId id;
        bool active;
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

    void deliver(const Event& event) {
        std::lock_guard<std::mutex> lock(subs_mutex_);
        for (const Slot& slot : slots_) {
            if (slot.active && slot.id == event.id) {
                slot.callback.function(event, slot.callback.context);
            }
        }
    }

    std::mutex queue_mutex_;  // guards queue_, head_, count_
    Event queue_[QueueDepth]{};
    std::size_t head_ = 0;
    std::size_t count_ = 0;

    std::mutex subs_mutex_;  // guards slots_
    Slot slots_[MaxSubscribers]{};
};

}  // namespace fw::bus
