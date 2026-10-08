#pragma once
// Task 3: IoT connection manager as a std::variant state machine.
//
// Each state is its own struct holding only the data that state needs.
// Transitions are overloaded lambdas applied with std::visit.
// Events that make no sense in a state are ignored and reported to a logger.

#include <cstdint>
#include <variant>

#include "fw/units.hpp"

namespace fw::conn {

// 64-bit milliseconds: deadline arithmetic cannot overflow (32-bit wraps after ~24.8 days).
using TimeMs = Quantity<MillisecondTag, std::int64_t>;

inline constexpr TimeMs kInitialBackoff{1000};       // 1 s
inline constexpr TimeMs kMaxBackoff{30000};          // 30 s cap
inline constexpr std::uint32_t kMaxFailures = 5;     // the 5th failure goes to Error

// Delay after the n-th consecutive failure: 1 s, 2 s, 4 s, ... capped at 30 s.
constexpr TimeMs backoff_delay(std::uint32_t failures) {
    TimeMs d = kInitialBackoff;
    for (std::uint32_t i = 1; i < failures && d < kMaxBackoff; ++i) {
        d = d * 2;
    }
    return d < kMaxBackoff ? d : kMaxBackoff;
}

enum class ErrorCode : std::uint8_t { None, RetriesExhausted };

// States (`name` is used for logging)
struct Idle { static constexpr const char* name = "Idle"; };
struct Connecting { static constexpr const char* name = "Connecting"; std::uint32_t attempt{1}; };
struct Connected { static constexpr const char* name = "Connected"; TimeMs since{}; };
struct Backoff {
    static constexpr const char* name = "Backoff";
    TimeMs until{};
    std::uint32_t failures{0};
};
struct Error { static constexpr const char* name = "Error"; ErrorCode code{ErrorCode::None}; };

using State = std::variant<Idle, Connecting, Connected, Backoff, Error>;

// Events
namespace ev {
struct Connect { static constexpr const char* name = "Connect"; };
struct Success { static constexpr const char* name = "Success"; };
struct Failure { static constexpr const char* name = "Failure"; };
struct Timeout { static constexpr const char* name = "Timeout"; };
struct LinkLost { static constexpr const char* name = "LinkLost"; };
struct Disconnect { static constexpr const char* name = "Disconnect"; };
struct Reset { static constexpr const char* name = "Reset"; };
struct Tick { static constexpr const char* name = "Tick"; TimeMs now{}; };
}  // namespace ev

using Event = std::variant<ev::Connect, ev::Success, ev::Failure, ev::Timeout,
                           ev::LinkLost, ev::Disconnect, ev::Reset, ev::Tick>;

// Injected dependencies. The destructors are protected: nobody deletes these
// through a base pointer, so they do not need to be virtual.
class IClock {
public:
    virtual TimeMs now() const = 0;

protected:
    ~IClock() = default;
};

class ILogger {
public:
    virtual void on_ignored(const char* state, const char* event) = 0;

protected:
    ~ILogger() = default;
};

// The "overloaded lambdas" helper (C++17): one object that has every lambda's operator().
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

class ConnectionManager {
public:
    ConnectionManager(const IClock& clock, ILogger& logger) : clock_{clock}, logger_{logger} {}

    const State& state() const { return state_; }

    void handle(const Event& event) {
        state_ = std::visit(
            overloaded{
                // Idle --Connect--> Connecting(1)
                [](const Idle&, const ev::Connect&) -> State { return Connecting{1}; },

                // Connecting --Success--> Connected(since = now)
                [this](const Connecting&, const ev::Success&) -> State {
                    return Connected{clock_.now()};
                },

                // Connecting --Failure or Timeout--> Backoff (Error on the 5th failure)
                [this](const Connecting& s, const ev::Failure&) -> State {
                    return enter_backoff_or_error(s.attempt);
                },
                [this](const Connecting& s, const ev::Timeout&) -> State {
                    return enter_backoff_or_error(s.attempt);
                },

                // Connected --LinkLost--> Backoff
                [this](const Connected&, const ev::LinkLost&) -> State {
                    return enter_backoff_or_error(1);
                },

                // Connected --Disconnect--> Idle
                [](const Connected&, const ev::Disconnect&) -> State { return Idle{}; },

                // Backoff --Tick--> Connecting(failures + 1) once the delay has expired
                [](const Backoff& s, const ev::Tick& t) -> State {
                    if (t.now >= s.until) {
                        return Connecting{s.failures + 1};
                    }
                    return s;
                },

                // Error --Reset--> Idle
                [](const Error&, const ev::Reset&) -> State { return Idle{}; },

                // Every other pair: ignore it and log it. Never crash.
                [this](const auto& s, const auto& e) -> State {
                    logger_.on_ignored(s.name, e.name);
                    return s;
                }},
            state_, event);
    }

private:
    // `failures` counts consecutive failures, including the one that just happened.
    State enter_backoff_or_error(std::uint32_t failures) const {
        if (failures >= kMaxFailures) {
            return Error{ErrorCode::RetriesExhausted};
        }
        return Backoff{clock_.now() + backoff_delay(failures), failures};
    }

    const IClock& clock_;
    ILogger& logger_;
    State state_{};  // starts as Idle (the first alternative)
};

}  // namespace fw::conn
