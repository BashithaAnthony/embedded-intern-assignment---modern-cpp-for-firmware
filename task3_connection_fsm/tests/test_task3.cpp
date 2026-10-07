#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>
#include <type_traits>
#include <variant>

#include "fw/connection_fsm.hpp"

using namespace fw::conn;

// --- test doubles --------------------------------------------------------------
class FakeClock final : public IClock {
public:
    TimeMs now() const override { return t_; }
    void set(TimeMs t) { t_ = t; }
    void advance(TimeMs d) { t_ += d; }

private:
    TimeMs t_{};
};

class RecordingLogger final : public ILogger {
public:
    unsigned ignored = 0;
    unsigned transitions = 0;
    StateKind ignored_state = StateKind::Idle;
    EventKind ignored_event = EventKind::Connect;
    StateKind from = StateKind::Idle;
    StateKind to = StateKind::Idle;

    void on_ignored(StateKind s, EventKind e) override {
        ++ignored;
        ignored_state = s;
        ignored_event = e;
    }
    void on_transition(StateKind f, StateKind t) override {
        ++transitions;
        from = f;
        to = t;
    }
};

constexpr TimeMs ms(std::int64_t v) { return TimeMs{v}; }

struct Fixture {
    FakeClock clock;
    RecordingLogger log;
    ConnectionManager fsm{clock, log};

    void tick_after(TimeMs d) {
        clock.advance(d);
        fsm.handle(ev::Tick{clock.now()});
    }
    template <typename T>
    const T* is() const { return std::get_if<T>(&fsm.state()); }
};

// Drive a fresh manager into the requested state using only valid transitions.
void drive_to(ConnectionManager& m, FakeClock& c, StateKind target) {
    switch (target) {
        case StateKind::Idle:
            break;
        case StateKind::Connecting:
            m.handle(ev::Connect{});
            break;
        case StateKind::Connected:
            m.handle(ev::Connect{});
            m.handle(ev::Success{});
            break;
        case StateKind::Backoff:
            m.handle(ev::Connect{});
            m.handle(ev::Failure{});
            break;
        case StateKind::Error:
            m.handle(ev::Connect{});
            m.handle(ev::Failure{});
            for (int i = 0; i < 4; ++i) {
                c.advance(ms(30000));
                m.handle(ev::Tick{c.now()});
                m.handle(ev::Failure{});
            }
            break;
    }
}

// --- compile-time checks -----------------------------------------------------------
static_assert(std::variant_size_v<State> == 5, "five states");
static_assert(std::variant_size_v<Event> == 8, "eight events");
static_assert(backoff_delay(1) == ms(1000), "1 s");
static_assert(backoff_delay(2) == ms(2000), "2 s");
static_assert(backoff_delay(4) == ms(8000), "8 s");

// --- tests ---------------------------------------------------------------------------
TEST_CASE("starts in Idle") {
    Fixture f;
    CHECK((f.fsm.kind() == StateKind::Idle));
    CHECK(f.is<Idle>() != nullptr);
}

TEST_CASE("arrow: Idle --Connect--> Connecting(attempt 1)") {
    Fixture f;
    f.fsm.handle(ev::Connect{});
    const Connecting* c = f.is<Connecting>();
    CHECK(c != nullptr);
    if (c) CHECK(c->attempt == 1u);
    CHECK(f.log.transitions == 1u);
    CHECK((f.log.from == StateKind::Idle));
    CHECK((f.log.to == StateKind::Connecting));
}

TEST_CASE("arrow: Connecting --Success--> Connected(since = clock time)") {
    Fixture f;
    f.clock.set(ms(5000));
    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Success{});
    const Connected* c = f.is<Connected>();
    CHECK(c != nullptr);
    if (c) CHECK((c->since == ms(5000)));
}

TEST_CASE("arrow: Connecting --Failure--> Backoff(until = now + 1 s, failures 1)") {
    Fixture f;
    f.clock.set(ms(1000));
    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Failure{});
    const Backoff* b = f.is<Backoff>();
    CHECK(b != nullptr);
    if (b) {
        CHECK((b->until == ms(2000)));
        CHECK(b->failures == 1u);
    }
}

TEST_CASE("arrow: Connecting --Timeout--> Backoff, same as Failure") {
    Fixture f;
    f.clock.set(ms(1000));
    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Timeout{});
    const Backoff* b = f.is<Backoff>();
    CHECK(b != nullptr);
    if (b) {
        CHECK((b->until == ms(2000)));
        CHECK(b->failures == 1u);
    }
}

TEST_CASE("arrow: Backoff --delay expired--> Connecting(attempt + 1); early ticks do nothing") {
    Fixture f;
    f.clock.set(ms(1000));
    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Failure{});             // until = 2000
    const unsigned before = f.log.transitions;

    f.tick_after(ms(999));                    // now = 1999: not yet
    CHECK((f.fsm.kind() == StateKind::Backoff));
    CHECK(f.log.transitions == before);       // no transition logged
    CHECK(f.log.ignored == 0u);               // an early tick is handled, not "ignored"

    f.tick_after(ms(1));                      // now = 2000: expired (>=)
    const Connecting* c = f.is<Connecting>();
    CHECK(c != nullptr);
    if (c) CHECK(c->attempt == 2u);
}

TEST_CASE("arrow: Connected --LinkLost--> Backoff") {
    Fixture f;
    f.clock.set(ms(100));
    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Success{});
    f.clock.set(ms(900));
    f.fsm.handle(ev::LinkLost{});
    const Backoff* b = f.is<Backoff>();
    CHECK(b != nullptr);
    if (b) {
        CHECK((b->until == ms(1900)));
        CHECK(b->failures == 1u);
    }
}

TEST_CASE("arrow: Connected --Disconnect--> Idle") {
    Fixture f;
    drive_to(f.fsm, f.clock, StateKind::Connected);
    f.fsm.handle(ev::Disconnect{});
    CHECK((f.fsm.kind() == StateKind::Idle));
}

TEST_CASE("backoff doubles 1,2,4,8 s and the 5th failure goes to Error") {
    Fixture f;
    f.clock.set(ms(1000));
    f.fsm.handle(ev::Connect{});

    const std::int64_t expected_delay[] = {1000, 2000, 4000, 8000};
    for (unsigned i = 0; i < 4; ++i) {
        const TimeMs failed_at = f.clock.now();
        f.fsm.handle(ev::Failure{});
        const Backoff* b = f.is<Backoff>();
        CHECK(b != nullptr);
        if (!b) return;
        CHECK(b->failures == i + 1);
        CHECK((b->until == failed_at + ms(expected_delay[i])));
        f.tick_after(ms(expected_delay[i]));  // wait exactly the delay
        const Connecting* c = f.is<Connecting>();
        CHECK(c != nullptr);
        if (c) CHECK(c->attempt == i + 2);
    }

    f.fsm.handle(ev::Failure{});  // the 5th consecutive failure
    const Error* e = f.is<Error>();
    CHECK(e != nullptr);
    if (e) CHECK((e->code == ErrorCode::RetriesExhausted));
}

TEST_CASE("5th failure via Timeout also goes to Error") {
    Fixture f;
    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Timeout{});
    for (int i = 0; i < 3; ++i) {
        f.tick_after(ms(30000));
        f.fsm.handle(ev::Timeout{});
    }
    f.tick_after(ms(30000));
    f.fsm.handle(ev::Timeout{});
    CHECK((f.fsm.kind() == StateKind::Error));
}

TEST_CASE("arrow: Error --Reset--> Idle, and the failure count starts over") {
    Fixture f;
    drive_to(f.fsm, f.clock, StateKind::Error);
    f.fsm.handle(ev::Reset{});
    CHECK((f.fsm.kind() == StateKind::Idle));
    f.fsm.handle(ev::Connect{});
    const Connecting* c = f.is<Connecting>();
    CHECK(c != nullptr);
    if (c) CHECK(c->attempt == 1u);
}

TEST_CASE("Success resets the consecutive-failure count") {
    Fixture f;
    f.clock.set(ms(0));
    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Failure{});             // failures = 1
    f.tick_after(ms(1000));                   // Connecting(2)
    f.fsm.handle(ev::Failure{});             // failures = 2
    f.tick_after(ms(2000));                   // Connecting(3)
    f.fsm.handle(ev::Success{});             // count resets here
    f.fsm.handle(ev::LinkLost{});
    const Backoff* b = f.is<Backoff>();
    CHECK(b != nullptr);
    if (b) CHECK(b->failures == 1u);          // not 3
}

TEST_CASE("backoff_delay is capped at 30 s") {
    CHECK((backoff_delay(0) == ms(1000)));
    CHECK((backoff_delay(4) == ms(8000)));
    CHECK((backoff_delay(5) == ms(16000)));
    CHECK((backoff_delay(6) == ms(30000)));   // 32 s would exceed the cap
    CHECK((backoff_delay(1000) == ms(30000)));
}

TEST_CASE("ignored events change nothing and are logged (from Idle)") {
    Fixture f;
    f.fsm.handle(ev::Success{});
    CHECK((f.fsm.kind() == StateKind::Idle));
    CHECK(f.log.ignored == 1u);
    CHECK((f.log.ignored_state == StateKind::Idle));
    CHECK((f.log.ignored_event == EventKind::Success));

    f.fsm.handle(ev::Failure{});
    f.fsm.handle(ev::Disconnect{});
    CHECK((f.fsm.kind() == StateKind::Idle));
    CHECK(f.log.ignored == 3u);
    CHECK(f.log.transitions == 0u);
}

TEST_CASE("ignored events from Connected and Error, including a stray Tick") {
    Fixture f;
    drive_to(f.fsm, f.clock, StateKind::Connected);
    f.fsm.handle(ev::Connect{});              // already connected
    CHECK((f.fsm.kind() == StateKind::Connected));
    CHECK(f.log.ignored == 1u);
    CHECK((f.log.ignored_event == EventKind::Connect));

    Fixture g;
    drive_to(g.fsm, g.clock, StateKind::Error);
    const unsigned before = g.log.ignored;
    g.fsm.handle(ev::Tick{g.clock.now()});
    g.fsm.handle(ev::Connect{});
    CHECK((g.fsm.kind() == StateKind::Error));
    CHECK(g.log.ignored == before + 2);
}

TEST_CASE("Disconnect while Backoff or Connecting is ignored (Figure 1 only allows it when Connected)") {
    Fixture f;
    drive_to(f.fsm, f.clock, StateKind::Backoff);
    f.fsm.handle(ev::Disconnect{});
    CHECK((f.fsm.kind() == StateKind::Backoff));
    CHECK(f.log.ignored == 1u);
}

TEST_CASE("all 40 state/event pairs: 8 are handled, 32 are ignored and logged, none crash") {
    RecordingLogger log;
    const StateKind states[] = {StateKind::Idle, StateKind::Connecting, StateKind::Connected,
                                StateKind::Backoff, StateKind::Error};
    for (const StateKind s : states) {
        for (unsigned e = 0; e < 8; ++e) {
            FakeClock clock;
            ConnectionManager fsm{clock, log};
            drive_to(fsm, clock, s);
            CHECK((fsm.kind() == s));  // setup reached the intended state

            const Event events[] = {ev::Connect{}, ev::Success{},    ev::Failure{},
                                    ev::Timeout{}, ev::LinkLost{},   ev::Disconnect{},
                                    ev::Reset{},   ev::Tick{clock.now()}};
            fsm.handle(events[e]);
            CHECK(fsm.state().index() < 5u);  // still a valid state
        }
    }
    CHECK(log.ignored == 32u);
}
