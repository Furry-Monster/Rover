#include "core/event/event_bus.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>
#include <vector>

// ===========================================================================
// Test event types
// ===========================================================================

struct ResizeEvent
{
    int width;
    int height;
};

struct CloseEvent
{
};

struct ValueEvent
{
    std::string value;
};

// ===========================================================================
// Basic subscribe + publish
// ===========================================================================
TEST_CASE("EventBus: subscribe and publish")
{
    EventBus bus;

    int received_w = 0;
    int received_h = 0;

    (void)bus.subscribe<ResizeEvent>([&](const ResizeEvent& e) {
        received_w = e.width;
        received_h = e.height;
    });

    bus.publish(ResizeEvent{800, 600});

    CHECK(received_w == 800);
    CHECK(received_h == 600);
}

TEST_CASE("EventBus: multiple subscribers")
{
    EventBus bus;

    int count = 0;
    (void)bus.subscribe<CloseEvent>([&](const CloseEvent&) { count++; });
    (void)bus.subscribe<CloseEvent>([&](const CloseEvent&) { count++; });
    (void)bus.subscribe<CloseEvent>([&](const CloseEvent&) { count++; });

    bus.publish(CloseEvent{});
    CHECK(count == 3);
}

TEST_CASE("EventBus: different event types are isolated")
{
    EventBus bus;

    bool resize_hit = false;
    bool close_hit  = false;

    (void)bus.subscribe<ResizeEvent>([&](const ResizeEvent&) { resize_hit = true; });
    (void)bus.subscribe<CloseEvent>([&](const CloseEvent&) { close_hit = true; });

    bus.publish(ResizeEvent{1, 1});

    CHECK(resize_hit);
    CHECK_FALSE(close_hit);
}

// ===========================================================================
// Unsubscribe
// ===========================================================================
TEST_CASE("EventBus: unsubscribe stops delivery")
{
    EventBus bus;

    int count = 0;
    auto id   = bus.subscribe<CloseEvent>([&](const CloseEvent&) { count++; });

    bus.publish(CloseEvent{});
    CHECK(count == 1);

    bus.unsubscribe(id);
    bus.publish(CloseEvent{});
    CHECK(count == 1);
}

TEST_CASE("EventBus: unsubscribe non-existent id is safe")
{
    EventBus bus;
    bus.unsubscribe(999);
}

// ===========================================================================
// Publish with no subscribers
// ===========================================================================
TEST_CASE("EventBus: publish with no subscribers is safe")
{
    EventBus bus;
    bus.publish(ResizeEvent{1920, 1080});
}

// ===========================================================================
// Event data integrity
// ===========================================================================
TEST_CASE("EventBus: event data passes through correctly")
{
    EventBus bus;

    std::string received;
    (void)bus.subscribe<ValueEvent>([&](const ValueEvent& e) { received = e.value; });

    bus.publish(ValueEvent{"hello, rover!"});
    CHECK(received == "hello, rover!");
}

// ===========================================================================
// Handler IDs are unique
// ===========================================================================
TEST_CASE("EventBus: handler IDs are unique and monotonic")
{
    EventBus bus;

    auto id1 = bus.subscribe<CloseEvent>([](const CloseEvent&) {});
    auto id2 = bus.subscribe<CloseEvent>([](const CloseEvent&) {});
    auto id3 = bus.subscribe<ResizeEvent>([](const ResizeEvent&) {});

    CHECK(id1 != id2);
    CHECK(id2 != id3);
    CHECK(id1 < id2);
    CHECK(id2 < id3);
}
