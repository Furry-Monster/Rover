// Unit tests for core/event/.
//
// Exercise: Delegate small-buffer optimization paths (function pointer,
// lambda capturing nothing, lambda capturing by reference, member function),
// Signal connect/emit/disconnect, EventBus type-keyed routing.

#include <doctest/doctest.h>

#include "core/event/delegate.h"
#include "core/event/event_bus.h"
#include "core/event/signal.h"

#include <string>

using namespace rover;

namespace {

int g_free_call_count = 0;
int free_function_target(int v) {
    ++g_free_call_count;
    return v * 2;
}

struct Counter {
    int value = 0;
    void increment(int by) { value += by; }
    int multiply(int factor) const { return value * factor; }
};

}  // namespace

// ---------------------------------------------------------------------------
// Delegate
// ---------------------------------------------------------------------------
TEST_CASE("Delegate: empty contextually-converts to false") {
    Delegate<void()> empty;
    CHECK_FALSE(static_cast<bool>(empty));
}

TEST_CASE("Delegate: stores and invokes a no-capture lambda") {
    bool fired = false;
    Delegate<void()> d{[&fired]() { fired = true; }};
    REQUIRE(static_cast<bool>(d));
    d();
    CHECK(fired);
}

TEST_CASE("Delegate: returns a value from a stored lambda") {
    Delegate<int(int)> doubler{[](int x) { return x * 2; }};
    CHECK(doubler(7) == 14);
    CHECK(doubler(-3) == -6);
}

TEST_CASE("Delegate: from_function<&fn> wraps a free function") {
    g_free_call_count = 0;
    auto d = Delegate<int(int)>::from_function<&free_function_target>();
    REQUIRE(static_cast<bool>(d));
    CHECK(d(21) == 42);
    CHECK(g_free_call_count == 1);
}

TEST_CASE("Delegate: from_method<&Cls::m> binds member + object") {
    Counter c;
    auto inc = Delegate<void(int)>::from_method<&Counter::increment>(&c);
    inc(5);
    inc(7);
    CHECK(c.value == 12);

    auto mul = Delegate<int(int)>::from_method<&Counter::multiply>(&c);
    CHECK(mul(3) == 36);
}

TEST_CASE("Delegate: heap fallback for large captures") {
    // Capture a 1KB struct -- well above the 32-byte SBO budget; this must
    // route through the heap path without altering observable behavior.
    struct Big { char bytes[1024]; };
    Big big{};
    big.bytes[0] = 'X';
    big.bytes[1023] = 'Z';
    Delegate<char(int)> d{[big](int idx) -> char {
        return big.bytes[idx];
    }};
    CHECK(d(0)    == 'X');
    CHECK(d(1023) == 'Z');
}

TEST_CASE("Delegate: reset() clears the callable") {
    Delegate<int()> d{[]() { return 1; }};
    REQUIRE(static_cast<bool>(d));
    d.reset();
    CHECK_FALSE(static_cast<bool>(d));
}

TEST_CASE("Delegate: move transfers ownership") {
    int counter = 0;
    Delegate<void()> a{[&counter]() { ++counter; }};
    Delegate<void()> b = std::move(a);
    CHECK_FALSE(static_cast<bool>(a));   // moved-from
    REQUIRE(static_cast<bool>(b));
    b();
    b();
    CHECK(counter == 2);
}

// ---------------------------------------------------------------------------
// Signal
// ---------------------------------------------------------------------------
TEST_CASE("Signal: connected slots are invoked on emit") {
    Signal<int> s;
    int sum = 0;
    s.connect(Delegate<void(int)>{[&sum](int x) { sum += x; }});
    s.connect(Delegate<void(int)>{[&sum](int x) { sum += 10 * x; }});
    s.emit(3);
    CHECK(sum == 33);   // 3 + 30
}

TEST_CASE("Signal: disconnect removes a single slot by id") {
    Signal<> s;
    int a = 0, b = 0;
    SlotId id_a = s.connect(Delegate<void()>{[&]() { ++a; }});
    SlotId id_b = s.connect(Delegate<void()>{[&]() { ++b; }});
    (void)id_b;

    s.emit();
    CHECK(a == 1);
    CHECK(b == 1);
    CHECK(s.slot_count() == 2);

    s.disconnect(id_a);
    s.emit();
    CHECK(a == 1);   // unchanged
    CHECK(b == 2);
    CHECK(s.slot_count() == 1);
}

TEST_CASE("Signal: disconnect_all empties the slot list") {
    Signal<> s;
    s.connect(Delegate<void()>{[]() {}});
    s.connect(Delegate<void()>{[]() {}});
    REQUIRE(s.slot_count() == 2);
    s.disconnect_all();
    CHECK(s.slot_count() == 0);
    s.emit();   // does not crash
}

// ---------------------------------------------------------------------------
// EventBus
// ---------------------------------------------------------------------------
namespace {
struct DamageEvent { int amount; };
struct HealEvent   { int amount; };
}

TEST_CASE("EventBus: routes events by type, ignoring others") {
    EventBus bus;
    int total_damage = 0;
    int total_heal   = 0;

    bus.subscribe<DamageEvent>(
        Delegate<void(const DamageEvent&)>{
            [&](const DamageEvent& e) { total_damage += e.amount; }
        });
    bus.subscribe<HealEvent>(
        Delegate<void(const HealEvent&)>{
            [&](const HealEvent& e) { total_heal += e.amount; }
        });

    bus.publish(DamageEvent{5});
    bus.publish(HealEvent{2});
    bus.publish(DamageEvent{3});

    CHECK(total_damage == 8);
    CHECK(total_heal   == 2);
}

TEST_CASE("EventBus: unsubscribe stops further delivery") {
    EventBus bus;
    int hits = 0;
    SlotId id = bus.subscribe<DamageEvent>(
        Delegate<void(const DamageEvent&)>{
            [&](const DamageEvent&) { ++hits; }
        });
    bus.publish(DamageEvent{1});
    bus.unsubscribe<DamageEvent>(id);
    bus.publish(DamageEvent{1});
    CHECK(hits == 1);
}

TEST_CASE("EventBus: publishing an unsubscribed type is a no-op") {
    EventBus bus;
    bus.publish(DamageEvent{10});   // no subscribers; must not crash.
    CHECK(true);
}
