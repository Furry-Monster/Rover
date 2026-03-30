#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/variant/variant.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>
#include <vector>

// ===========================================================================
// Test classes
// ===========================================================================

class Emitter : public Object
{
    ROVER_CLASS(Emitter, Object)

public:
    static void _bind_methods()
    {
        ClassDB::add_signal(Emitter::get_class_static(), StringName("value_changed"));
        ClassDB::add_signal(Emitter::get_class_static(), StringName("completed"));
    }
};

class Receiver : public Object
{
    ROVER_CLASS(Receiver, Object)

public:
    int         call_count = 0;
    std::string last_value;

    Variant call(const StringName& p_method, const Variant* p_args, int p_arg_count) override
    {
        if (p_method == StringName("on_value_changed") && p_arg_count > 0)
        {
            last_value = p_args[0].as_string();
            call_count++;
        }
        else if (p_method == StringName("on_completed"))
        {
            call_count++;
        }
        return {};
    }
};

// ===========================================================================
// Setup
// ===========================================================================
TEST_CASE("Signal setup: register classes" * doctest::test_suite("setup"))
{
    ClassDB::register_class<Object>();
    ClassDB::register_class<Emitter>();
    ClassDB::register_class<Receiver>();
}

// ===========================================================================
// ClassDB signal registration
// ===========================================================================
TEST_CASE("ClassDB: has_signal")
{
    CHECK(ClassDB::has_signal(StringName("Emitter"), StringName("value_changed")));
    CHECK(ClassDB::has_signal(StringName("Emitter"), StringName("completed")));
    CHECK_FALSE(ClassDB::has_signal(StringName("Emitter"), StringName("nonexistent")));
}

// ===========================================================================
// connect + emit with Object+method Callable
// ===========================================================================
TEST_CASE("Signal: connect and emit with object+method")
{
    Emitter  emitter;
    Receiver receiver;

    emitter.connect(StringName("value_changed"), &receiver, StringName("on_value_changed"));

    Variant args[] = {Variant("hello")};
    emitter.emit_signal_argv(StringName("value_changed"), args, 1);

    CHECK(receiver.call_count == 1);
    CHECK(receiver.last_value == "hello");
}

TEST_CASE("Signal: emit with no connections is safe")
{
    Emitter emitter;
    emitter.emit_signal(StringName("value_changed"));
}

TEST_CASE("Signal: multiple connections")
{
    Emitter  emitter;
    Receiver r1, r2;

    emitter.connect(StringName("completed"), &r1, StringName("on_completed"));
    emitter.connect(StringName("completed"), &r2, StringName("on_completed"));

    emitter.emit_signal(StringName("completed"));

    CHECK(r1.call_count == 1);
    CHECK(r2.call_count == 1);
}

// ===========================================================================
// connect with lambda Callable
// ===========================================================================
TEST_CASE("Signal: connect with lambda")
{
    Emitter emitter;
    int     counter = 0;

    Callable fn([&counter](const Variant*, int) { counter++; });
    emitter.connect(StringName("completed"), fn);
    emitter.emit_signal(StringName("completed"));

    CHECK(counter == 1);
}

// ===========================================================================
// disconnect
// ===========================================================================
TEST_CASE("Signal: disconnect stops delivery")
{
    Emitter  emitter;
    Receiver receiver;

    emitter.connect(StringName("completed"), &receiver, StringName("on_completed"));
    emitter.emit_signal(StringName("completed"));
    CHECK(receiver.call_count == 1);

    emitter.disconnect(StringName("completed"), &receiver, StringName("on_completed"));
    emitter.emit_signal(StringName("completed"));
    CHECK(receiver.call_count == 1);
}

// ===========================================================================
// is_connected
// ===========================================================================
TEST_CASE("Signal: is_connected")
{
    Emitter  emitter;
    Receiver receiver;

    CHECK_FALSE(emitter.is_connected(StringName("completed"), &receiver, StringName("on_completed")));

    emitter.connect(StringName("completed"), &receiver, StringName("on_completed"));
    CHECK(emitter.is_connected(StringName("completed"), &receiver, StringName("on_completed")));

    emitter.disconnect(StringName("completed"), &receiver, StringName("on_completed"));
    CHECK_FALSE(emitter.is_connected(StringName("completed"), &receiver, StringName("on_completed")));
}

// ===========================================================================
// Variadic emit_signal template
// ===========================================================================
TEST_CASE("Signal: variadic emit_signal template")
{
    Emitter  emitter;
    Receiver receiver;

    emitter.connect(StringName("value_changed"), &receiver, StringName("on_value_changed"));

    emitter.emit_signal(StringName("value_changed"), std::string("world"));

    CHECK(receiver.call_count == 1);
    CHECK(receiver.last_value == "world");
}
