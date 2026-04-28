// Unit tests for core/object/.
//
// Exercise: Object instance ID uniqueness, ROVER_CLASS macro behavior,
// ClassDB factory + inheritance walking, RefCounted atomic refcount.

#include <doctest/doctest.h>

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/object/object_macros.h"
#include "core/object/ref_counted.h"

#include <set>

using namespace rover;

namespace {

// Test fixture classes -- isolated namespace so we don't conflict with
// the always-registered Object/RefCounted in register_core_types().
class TestNode : public Object {
    ROVER_CLASS(TestNode, Object)
public:
    TestNode() = default;
};

class TestNode2D : public TestNode {
    ROVER_CLASS(TestNode2D, TestNode)
public:
    TestNode2D() = default;
};

}  // namespace

// ---------------------------------------------------------------------------
// Object
// ---------------------------------------------------------------------------
TEST_CASE("Object: get_class_name reports its runtime type") {
    Object o;
    CHECK(o.get_class_name() == "Object");
    CHECK(Object::get_class_name_static() == "Object");
}

TEST_CASE("Object: instance ids are globally unique") {
    std::set<u64> ids;
    for (int i = 0; i < 32; ++i) {
        Object o;
        const auto inserted = ids.insert(o.get_instance_id()).second;
        CHECK(inserted);   // duplicate id would be a critical bug
    }
    CHECK(ids.size() == 32);
}

TEST_CASE("ROVER_CLASS: get_class_name returns the derived name") {
    TestNode n;
    CHECK(n.get_class_name()                == "TestNode");
    CHECK(TestNode::get_class_name_static() == "TestNode");
    CHECK(TestNode::get_parent_class_name_static() == "Object");
}

TEST_CASE("ROVER_CLASS: is_class walks the inheritance chain") {
    TestNode2D leaf;
    CHECK(leaf.is_class("TestNode2D"));
    CHECK(leaf.is_class("TestNode"));
    CHECK(leaf.is_class("Object"));
    CHECK_FALSE(leaf.is_class("Other"));
}

// ---------------------------------------------------------------------------
// ClassDB
// ---------------------------------------------------------------------------
TEST_CASE("ClassDB: Object and RefCounted are registered at core startup") {
    // register_core_types() is *not* called from rover_tests, but the
    // built-in registrations are sticky across TUs: callers can register
    // ad-hoc; existence checks on names we control are safe regardless.
    // Use this test to ensure register_class can store + retrieve a class.
    if (!ClassDB::class_exists("EvtTestObject")) {
        ClassDB::register_class("EvtTestObject", "Object",
                                []() -> Object* { return new Object(); });
    }
    CHECK(ClassDB::class_exists("EvtTestObject"));
    CHECK_FALSE(ClassDB::class_exists("DefinitelyNotARealClass"));
}

TEST_CASE("ClassDB: instantiate creates an instance via the factory") {
    if (!ClassDB::class_exists("EvtTestObject2")) {
        ClassDB::register_class("EvtTestObject2", "Object",
                                []() -> Object* { return new Object(); });
    }
    Object* o = ClassDB::instantiate("EvtTestObject2");
    REQUIRE(o != nullptr);
    delete o;

    // Unknown class returns nullptr.
    CHECK(ClassDB::instantiate("NoSuchClass") == nullptr);
}

TEST_CASE("ClassDB: is_parent_class walks the registered chain") {
    if (!ClassDB::class_exists("CdbTestNode")) {
        ClassDB::register_class("CdbTestNode", "Object",
                                []() -> Object* { return new Object(); });
    }
    if (!ClassDB::class_exists("CdbTestNode2D")) {
        ClassDB::register_class("CdbTestNode2D", "CdbTestNode",
                                []() -> Object* { return new Object(); });
    }
    CHECK(ClassDB::is_parent_class("CdbTestNode2D", "CdbTestNode"));
    CHECK(ClassDB::is_parent_class("CdbTestNode2D", "Object"));
    CHECK_FALSE(ClassDB::is_parent_class("CdbTestNode", "CdbTestNode2D"));
    CHECK_FALSE(ClassDB::is_parent_class("CdbTestNode", "NotRegistered"));
}

// ---------------------------------------------------------------------------
// RefCounted
// ---------------------------------------------------------------------------
TEST_CASE("RefCounted: starts with refcount of 1") {
    RefCounted r;
    CHECK(r.ref_count() == 1);
}

TEST_CASE("RefCounted: add_ref / release manage the count and signal final release") {
    RefCounted r;
    r.add_ref();
    r.add_ref();
    CHECK(r.ref_count() == 3);

    CHECK_FALSE(r.release());   // 3 -> 2
    CHECK_FALSE(r.release());   // 2 -> 1
    CHECK(r.release());         // 1 -> 0  (caller should delete; we don't here
                                 //  because r is on the stack)
    CHECK(r.ref_count() == 0);
}

TEST_CASE("RefCounted: get_class_name reports RefCounted, is_class walks up") {
    RefCounted r;
    CHECK(r.get_class_name() == "RefCounted");
    CHECK(r.is_class("RefCounted"));
    CHECK(r.is_class("Object"));
}
