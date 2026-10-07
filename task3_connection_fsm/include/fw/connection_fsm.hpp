#pragma once
// Task 3: IoT connection manager as a std::variant state machine.
//
// Each state is its own struct holding only the data that state needs.
// Transitions are written as overloaded lambdas and applied with std::visit.
// Events that make no sense in a state are ignored and reported to a logger.
//
// No heap, no exceptions, no RTTI: std::variant lives inside the manager object.

#include <cstdint>
#include <type_traits>
#include <variant>

#include "fw/units.hpp"

namespace fw::conn {

// ---------------------------------------------------------------------------
// Time and backoff policy
// ---------------------------------------------------------------------------
// 64-bit milliseconds: no wrap-around for ~292 million years, so deadline
// arithmetic cannot overflow (a 32-bit counter wraps after ~24.8 days).
using TimeMs = Quantity<MillisecondTag, std::int64_t>;

inline constexpr TimeMs kInitialBackoff{1000};              // 1 s
inline constexpr TimeMs kMaxBackoff{30000};                 // 30 s cap
inline constexpr std::uint32_t kMaxConsecutiveFailures = 5;  // 5th failure -> Error

// Delay after the n-th consecutive failure: 1 s, 2 s, 4 s, ... capped at 30 s.
constexpr TimeMs backoff_delay(std::uint32_t failures) {
    TimeMs d = kInitialBackoff;
    for (std::uint32_t i = 1; i < failures && d < kMaxBackoff; ++i) {
        d = d * 2;  // stops doubling once at the cap, so it cannot overflow
    }
    return d < kMaxBackoff ? d : kMaxBackoff;
}

enum class ErrorCode : std::uint8_t { None, RetriesExhausted };

// ---------------------------------------------------------------------------
// States: each holds only its own data
// ---------------------------------------------------------------------------
struct Idle {};
struct Connecting { std::uint32_t attempt{1}; };               // attempt n (1-based)
struct Connected { TimeMs since{}; };                          // time of success
struct Backoff { TimeMs until{}; std::uint32_t failures{0}; }; // wait until `until`
struct Error { ErrorCode code{ErrorCode::None}; };

using State = std::variant<Idle, Connecting, Connected, Backoff, Error>;

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------
namespace ev {
struct Connect {};
struct Success {};
struct Failure {};
struct Timeout {};
struct LinkLost {};
struct Disconnect {};
struct Reset {};
struct Tick { TimeMs now{}; };
}  // namespace ev

using Event = std::variant<ev::Connect, ev::Success, ev::Failure, ev::Timeout,
                           ev::LinkLost, ev::Disconnect, ev::Reset, ev::Tick>;

// ---------------------------------------------------------------------------
// Names for logging (no strings are built: only pointers to literals)
// ---------------------------------------------------------------------------
enum class StateKind : std::uint8_t { Idle, Connecting, Connected, Backoff, Error };
enum class EventKind : std::uint8_t {
    Connect, Success, Failure, Timeout, LinkLost, Disconnect, Reset, Tick
};

constexpr StateKind state_kind(const Idle&) { return StateKind::Idle; }
constexpr StateKind state_kind(const Connecting&) { return StateKind::Connecting; }
constexpr StateKind state_kind(const Connected&) { return StateKind::Connected; }
constexpr StateKind state_kind(const Backoff&) { return StateKind::Backoff; }
constexpr StateKind state_kind(const Error&) { return StateKind::Error; }

constexpr EventKind event_kind(const ev::Connect&) { return EventKind::Connect; }
constexpr EventKind event_kind(const ev::Success&) { return EventKind::Success; }
constexpr EventKind event_kind(const ev::Failure&) { return EventKind::Failure; }
constexpr EventKind event_kind(const ev::Timeout&) { return EventKind::Timeout; }
constexpr EventKind event_kind(const ev::LinkLost&) { return EventKind::LinkLost; }
constexpr EventKind event_kind(const ev::Disconnect&) { return EventKind::Disconnect; }
constexpr EventKind event_kind(const ev::Reset&) { return EventKind::Reset; }
constexpr EventKind event_kind(const ev::Tick&) { return EventKind::Tick; }

inline StateKind kind_of(const State& s) {
    return std::visit([](const auto& alt) { return state_kind(alt); }, s);
}
inline EventKind kind_of(const Event& e) {
    return std::visit([](const auto& alt) { return event_kind(alt); }, e);
}

constexpr const char* to_string(StateKind k) {
    switch (k) {
        case StateKind::Idle: return "Idle";
        case StateKind::Connecting: return "Connecting";
        case StateKind::Connected: return "Connected";
        case StateKind::Backoff: return "Backoff";
        case StateKind::Error: return "Error";
    }
    return "?";
}
constexpr const char* to_string(EventKind k) {
    switch (k) {
        case EventKind::Connect: return "Connect";
        case EventKind::Success: return "Success";
        case EventKind::Failure: return "Failure";
        case EventKind::Timeout: return "Timeout";
        case EventKind::LinkLost: return "LinkLost";
        case EventKind::Disconnect: return "Disconnect";
        case EventKind::Reset: return "Reset";
        case EventKind::Tick: return "Tick";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Injected dependencies: a clock and a logger.
// Abstract interfaces with a protected, non-virtual destructor: nobody deletes
// these through a base pointer, so no vtable-deleting-destructor is needed.
// Copying is deleted to prevent slicing.
// ---------------------------------------------------------------------------
class IClock {
public:
    IClock(const IClock&) = delete;
    IClock& operator=(const IClock&) = delete;
    virtual TimeMs now() const = 0;

protected:
    IClock() = default;
    ~IClock() = default;
};

class ILogger {
public:
    ILogger(const ILogger&) = delete;
    ILogger& operator=(const ILogger&) = delete;
    // An event arrived that the current state does not handle.
    virtual void on_ignored(StateKind, EventKind) {}
    // The machine moved to a different kind of state.
    virtual void on_transition(StateKind /*from*/, StateKind /*to*/) {}

protected:
    ILogger() = default;
    ~ILogger() = default;
};

class NullLogger final : public ILogger {};

// ---------------------------------------------------------------------------
// The "overloaded lambdas" helper (C++17)
// ---------------------------------------------------------------------------
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// ---------------------------------------------------------------------------
// The state machine
// ---------------------------------------------------------------------------
class ConnectionManager {
public:
    ConnectionManager(const IClock& clock, ILogger& logger) : clock_{clock}, logger_{logger} {}

    [[nodiscard]] const State& state() const { return state_; }
    [[nodiscard]] StateKind kind() const { return kind_of(state_); }

    void handle(const Event& event) {
        const State next = std::visit(
            overloaded{
                // Idle --Connect--> Connecting(1)
                [](const Idle&, const ev::Connect&) -> State { return Connecting{1}; },

                // Connecting --Success--> Connected(since = now)
                [this](const Connecting&, const ev::Success&) -> State {
                    return Connected{clock_.now()};
                },

                // Connecting --Failure / Timeout--> Backoff (or Error on the 5th failure)
                [this](const Connecting& s, const ev::Failure&) -> State {
                    return enter_backoff_or_error(s.attempt);
                },
                [this](const Connecting& s, const ev::Timeout&) -> State {
                    return enter_backoff_or_error(s.attempt);
                },

                // Connected --LinkLost--> Backoff (first failure since the last Success)
                [this](const Connected&, const ev::LinkLost&) -> State {
                    return enter_backoff_or_error(1);
                },

                // Connected --Disconnect--> Idle (user request)
                [](const Connected&, const ev::Disconnect&) -> State { return Idle{}; },

                // Backoff --Tick(now >= until)--> Connecting(failures + 1); earlier ticks do nothing
                [](const Backoff& s, const ev::Tick& t) -> State {
                    if (t.now >= s.until) {
                        return Connecting{s.failures + 1};
                    }
                    return s;
                },

                // Error --Reset--> Idle
                [](const Error&, const ev::Reset&) -> State { return Idle{}; },

                // Everything else: ignore and log, never crash.
                [this](const auto& s, const auto& e) -> State {
                    logger_.on_ignored(state_kind(s), event_kind(e));
                    return s;
                }},
            state_, event);

        if (next.index() != state_.index()) {
            logger_.on_transition(kind_of(state_), kind_of(next));
        }
        state_ = next;
    }

private:
    // `failures` counts consecutive failures including the one that just happened.
    // Entering Backoff records it; reaching the limit goes to Error instead.
    State enter_backoff_or_error(std::uint32_t failures) const {
        if (failures >= kMaxConsecutiveFailures) {
            return Error{ErrorCode::RetriesExhausted};
        }
        return Backoff{clock_.now() + backoff_delay(failures), failures};
    }

    const IClock& clock_;
    ILogger& logger_;
    State state_{};  // starts in Idle (first alternative)
};

}  // namespace fw::conn
