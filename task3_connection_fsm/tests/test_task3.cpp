#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <variant>

#include "fw/connection_fsm.hpp"

using namespace fw::conn;

// Fake clock: the test sets the time by hand, so no test ever waits.
class FakeClock final : public IClock {
public:
    TimeMs now() const override { return t_; }
    void advance(TimeMs d) { t_ = t_ + d; }

private:
    TimeMs t_{};
};

// Logger that remembers how many events were ignored, and the last one.
class RecordingLogger final : public ILogger {
public:
    unsigned ignored = 0;
    const char* last_state = "";
    const char* last_event = "";

    void on_ignored(const char* state, const char* event) override {
        ++ignored;
        last_state = state;
        last_event = event;
    }
};

constexpr TimeMs ms(std::int64_t v) { return TimeMs{v}; }

struct Fixture {
    FakeClock clock;
    RecordingLogger log;
    ConnectionManager fsm{clock, log};

    template <typename T>
    const T* is() const { return std::get_if<T>(&fsm.state()); }

    void tick_after(TimeMs d) {
        clock.advance(d);
        fsm.handle(ev::Tick{clock.now()});
    }
};

// Connect, then fail five times in a row.
void reach_error(Fixture& f) {
    f.fsm.handle(ev::Connect{});
    for (int i = 0; i < 4; ++i) {
        f.fsm.handle(ev::Failure{});
        f.tick_after(ms(30000));  // wait longer than any delay
    }
    f.fsm.handle(ev::Failure{});
}

TEST_CASE("Idle --Connect--> Connecting(attempt 1)") {
    Fixture f;
    CHECK(f.is<Idle>() != nullptr);
    f.fsm.handle(ev::Connect{});
    const Connecting* c = f.is<Connecting>();
    CHECK(c != nullptr);
    if (c) CHECK(c->attempt == 1u);
}

TEST_CASE("Connecting --Success--> Connected(since = clock time)") {
    Fixture f;
    f.clock.advance(ms(5000));
    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Success{});
    const Connected* c = f.is<Connected>();
    CHECK(c != nullptr);
    if (c) CHECK((c->since == ms(5000)));
}

TEST_CASE("Connecting --Failure or Timeout--> Backoff(1 s, failures 1)") {
    Fixture f;
    f.clock.advance(ms(1000));
    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Failure{});
    const Backoff* b = f.is<Backoff>();
    CHECK(b != nullptr);
    if (b) {
        CHECK((b->until == ms(2000)));
        CHECK(b->failures == 1u);
    }

    Fixture g;
    g.fsm.handle(ev::Connect{});
    g.fsm.handle(ev::Timeout{});  // Timeout behaves like Failure
    CHECK(g.is<Backoff>() != nullptr);
}

TEST_CASE("Backoff --Tick--> Connecting only after the delay has expired") {
    Fixture f;
    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Failure{});  // until = 1000 ms
    f.tick_after(ms(999));        // too early
    CHECK(f.is<Backoff>() != nullptr);
    f.tick_after(ms(1));          // exactly at the deadline
    const Connecting* c = f.is<Connecting>();
    CHECK(c != nullptr);
    if (c) CHECK(c->attempt == 2u);
}

TEST_CASE("Connected --LinkLost--> Backoff") {
    Fixture f;
    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Success{});
    f.fsm.handle(ev::LinkLost{});
    const Backoff* b = f.is<Backoff>();
    CHECK(b != nullptr);
    if (b) CHECK(b->failures == 1u);
}

TEST_CASE("Connected --Disconnect--> Idle") {
    Fixture f;
    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Success{});
    f.fsm.handle(ev::Disconnect{});
    CHECK(f.is<Idle>() != nullptr);
}

TEST_CASE("backoff doubles 1, 2, 4, 8 s and the 5th failure goes to Error") {
    Fixture f;
    f.fsm.handle(ev::Connect{});
    const std::int64_t delays[] = {1000, 2000, 4000, 8000};
    for (unsigned i = 0; i < 4; ++i) {
        const TimeMs failed_at = f.clock.now();
        f.fsm.handle(ev::Failure{});
        const Backoff* b = f.is<Backoff>();
        CHECK(b != nullptr);
        if (!b) return;
        CHECK(b->failures == i + 1);
        CHECK((b->until == failed_at + ms(delays[i])));
        f.tick_after(ms(delays[i]));  // wait exactly the delay
    }
    f.fsm.handle(ev::Failure{});      // the 5th failure
    const Error* e = f.is<Error>();
    CHECK(e != nullptr);
    if (e) CHECK((e->code == ErrorCode::RetriesExhausted));

    CHECK((backoff_delay(6) == ms(30000)));  // 32 s would pass the cap
}

TEST_CASE("Error --Reset--> Idle") {
    Fixture f;
    reach_error(f);
    CHECK(f.is<Error>() != nullptr);
    f.fsm.handle(ev::Reset{});
    CHECK(f.is<Idle>() != nullptr);
}

TEST_CASE("Success resets the failure count") {
    Fixture f;
    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Failure{});  // failures 1
    f.tick_after(ms(1000));
    f.fsm.handle(ev::Failure{});  // failures 2
    f.tick_after(ms(2000));
    f.fsm.handle(ev::Success{});  // the count starts over here
    f.fsm.handle(ev::LinkLost{});
    const Backoff* b = f.is<Backoff>();
    CHECK(b != nullptr);
    if (b) CHECK(b->failures == 1u);  // not 3
}

TEST_CASE("ignored events change nothing and are logged") {
    Fixture f;
    f.fsm.handle(ev::Success{});   // makes no sense in Idle
    f.fsm.handle(ev::Failure{});   // makes no sense in Idle
    CHECK(f.is<Idle>() != nullptr);
    CHECK(f.log.ignored == 2u);
    CHECK(std::strcmp(f.log.last_state, "Idle") == 0);
    CHECK(std::strcmp(f.log.last_event, "Failure") == 0);

    f.fsm.handle(ev::Connect{});
    f.fsm.handle(ev::Success{});
    f.fsm.handle(ev::Connect{});   // already connected
    CHECK(f.is<Connected>() != nullptr);
    CHECK(f.log.ignored == 3u);
}
