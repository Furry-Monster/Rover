#include "core/object/class_db.h"
#include "core/object/object.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <vector>

// ===========================================================================
// Test hierarchy:  Object -> RefCounted -> Resource -> Texture
//                                        \-> Material
// ===========================================================================

class RefCounted : public Object
{
    ROVER_CLASS(RefCounted, Object)

protected:
    void _notification(int p_what) { notifications.push_back(p_what); }

public:
    std::vector<int> notifications;
};

class Resource : public RefCounted
{
    ROVER_CLASS(Resource, RefCounted)

protected:
    void _notification(int p_what) { notifications_res.push_back(p_what); }

public:
    std::vector<int> notifications_res;
};

// Does NOT override _notification — dispatch should skip it.
class Texture : public Resource
{
    ROVER_CLASS(Texture, Resource)
};

class Material : public Resource
{
    ROVER_CLASS(Material, Resource)
};

// ===========================================================================
// One-time class registration (runs before any test case)
// ===========================================================================
TEST_CASE("ClassDB: register test hierarchy" * doctest::test_suite("setup"))
{
    ClassDB::register_class<Object>();
    ClassDB::register_class<RefCounted>();
    ClassDB::register_class<Resource>();
    ClassDB::register_class<Texture>();
    ClassDB::register_class<Material>();
}

// ===========================================================================
// RTTI — is_class / is_class_ptr / get_class_name
// ===========================================================================
TEST_CASE("RTTI: is_class walks full inheritance chain")
{
    Texture tex;

    CHECK(tex.is_class(StringName("Texture")));
    CHECK(tex.is_class(StringName("Resource")));
    CHECK(tex.is_class(StringName("RefCounted")));
    CHECK(tex.is_class(StringName("Object")));
    CHECK_FALSE(tex.is_class(StringName("Material")));
    CHECK_FALSE(tex.is_class(StringName("Nonexistent")));
}

TEST_CASE("RTTI: is_class_ptr with static pointers")
{
    Texture tex;

    CHECK(tex.is_class_ptr(Texture::get_class_ptr_static()));
    CHECK(tex.is_class_ptr(Resource::get_class_ptr_static()));
    CHECK(tex.is_class_ptr(RefCounted::get_class_ptr_static()));
    CHECK(tex.is_class_ptr(Object::get_class_ptr_static()));
    CHECK_FALSE(tex.is_class_ptr(Material::get_class_ptr_static()));
}

TEST_CASE("RTTI: get_class_name returns most-derived name")
{
    Texture tex;
    CHECK(tex.get_class_name() == StringName("Texture"));

    Resource res;
    CHECK(res.get_class_name() == StringName("Resource"));

    Object obj;
    CHECK(obj.get_class_name() == StringName("Object"));
}

TEST_CASE("RTTI: get_class_static returns correct StringName")
{
    CHECK(Texture::get_class_static() == StringName("Texture"));
    CHECK(Resource::get_class_static() == StringName("Resource"));
    CHECK(RefCounted::get_class_static() == StringName("RefCounted"));
    CHECK(Object::get_class_static() == StringName("Object"));
}

// ===========================================================================
// cast_to
// ===========================================================================
TEST_CASE("cast_to: valid upcast and cross-cast")
{
    Resource res;
    Object*  obj = &res;

    SUBCASE("cast to exact type succeeds")
    {
        CHECK(Object::cast_to<Resource>(obj) == &res);
    }

    SUBCASE("cast to base type succeeds")
    {
        CHECK(Object::cast_to<RefCounted>(obj) == &res);
    }

    SUBCASE("cast to unrelated sibling returns nullptr")
    {
        CHECK(Object::cast_to<Texture>(obj) == nullptr);
    }

    SUBCASE("cast to more-derived type returns nullptr")
    {
        CHECK(Object::cast_to<Material>(obj) == nullptr);
    }
}

TEST_CASE("cast_to: nullptr input returns nullptr")
{
    CHECK(Object::cast_to<Resource>(static_cast<Object*>(nullptr)) == nullptr);
}

TEST_CASE("cast_to: const overload")
{
    const Resource  res;
    const Object*   obj = &res;
    const Resource* out = Object::cast_to<Resource>(obj);
    CHECK(out == &res);
}

// ===========================================================================
// ClassDB — registration & queries
// ===========================================================================
TEST_CASE("ClassDB: class_exists")
{
    CHECK(ClassDB::class_exists(StringName("Object")));
    CHECK(ClassDB::class_exists(StringName("RefCounted")));
    CHECK(ClassDB::class_exists(StringName("Resource")));
    CHECK(ClassDB::class_exists(StringName("Texture")));
    CHECK(ClassDB::class_exists(StringName("Material")));
    CHECK_FALSE(ClassDB::class_exists(StringName("Nonexistent")));
}

TEST_CASE("ClassDB: get_parent_class")
{
    CHECK(ClassDB::get_parent_class(StringName("Texture")) == StringName("Resource"));
    CHECK(ClassDB::get_parent_class(StringName("Material")) == StringName("Resource"));
    CHECK(ClassDB::get_parent_class(StringName("Resource")) == StringName("RefCounted"));
    CHECK(ClassDB::get_parent_class(StringName("RefCounted")) == StringName("Object"));
    CHECK(ClassDB::get_parent_class(StringName("Nonexistent")).empty());
}

TEST_CASE("ClassDB: is_parent_class")
{
    SUBCASE("reflexive — a class is its own parent")
    {
        CHECK(ClassDB::is_parent_class(StringName("Object"), StringName("Object")));
    }

    SUBCASE("transitive chain")
    {
        CHECK(ClassDB::is_parent_class(StringName("Texture"), StringName("Object")));
        CHECK(ClassDB::is_parent_class(StringName("Texture"), StringName("RefCounted")));
        CHECK(ClassDB::is_parent_class(StringName("Texture"), StringName("Resource")));
    }

    SUBCASE("not a parent")
    {
        CHECK_FALSE(ClassDB::is_parent_class(StringName("Object"), StringName("Texture")));
        CHECK_FALSE(ClassDB::is_parent_class(StringName("Material"), StringName("Texture")));
    }
}

TEST_CASE("ClassDB: get_class_info returns valid info")
{
    auto* info = ClassDB::get_class_info(StringName("Texture"));
    REQUIRE(info != nullptr);
    CHECK(info->name == StringName("Texture"));
    CHECK(info->parent_name == StringName("Resource"));
    CHECK(info->exposed);
    CHECK(info->creation_func != nullptr);
}

TEST_CASE("ClassDB: get_class_info returns nullptr for unknown")
{
    CHECK(ClassDB::get_class_info(StringName("Unknown")) == nullptr);
}

// ===========================================================================
// Instantiation
// ===========================================================================
TEST_CASE("ClassDB: instantiate by name")
{
    Object* obj = ClassDB::instantiate(StringName("Texture"));
    REQUIRE(obj != nullptr);
    CHECK(obj->get_class_name() == StringName("Texture"));
    CHECK(obj->is_class(StringName("Resource")));
    CHECK(obj->is_class(StringName("Object")));

    auto* tex = Object::cast_to<Texture>(obj);
    CHECK(tex != nullptr);

    memdelete(obj);
}

TEST_CASE("ClassDB: instantiate returns nullptr for unknown class")
{
    CHECK(ClassDB::instantiate(StringName("Nonexistent")) == nullptr);
}

TEST_CASE("ClassDB: instantiate returns nullptr for empty name")
{
    CHECK(ClassDB::instantiate(StringName()) == nullptr);
}

// ===========================================================================
// Notification dispatch
// ===========================================================================
TEST_CASE("Notification: forward dispatch (base -> derived)")
{
    Resource res;
    res.notification(42, /*p_reversed=*/false);

    CHECK(res.notifications.size() == 1);
    CHECK(res.notifications[0] == 42);
    CHECK(res.notifications_res.size() == 1);
    CHECK(res.notifications_res[0] == 42);
}

TEST_CASE("Notification: backward dispatch (derived -> base)")
{
    Resource res;
    res.notification(99, /*p_reversed=*/true);

    CHECK(res.notifications.size() == 1);
    CHECK(res.notifications[0] == 99);
    CHECK(res.notifications_res.size() == 1);
    CHECK(res.notifications_res[0] == 99);
}

TEST_CASE("Notification: non-overriding class is skipped")
{
    // Texture does NOT override _notification.
    Texture tex;
    tex.notification(7, false);

    // RefCounted and Resource handlers should each fire exactly once.
    CHECK(tex.notifications.size() == 1);
    CHECK(tex.notifications[0] == 7);
    CHECK(tex.notifications_res.size() == 1);
    CHECK(tex.notifications_res[0] == 7);
}

TEST_CASE("Notification: multiple notifications accumulate")
{
    RefCounted rc;
    rc.notification(1);
    rc.notification(2);
    rc.notification(3);

    REQUIRE(rc.notifications.size() == 3);
    CHECK(rc.notifications[0] == 1);
    CHECK(rc.notifications[1] == 2);
    CHECK(rc.notifications[2] == 3);
}

// ===========================================================================
// Instance identity
// ===========================================================================
TEST_CASE("Instance ID: each object gets a unique ID")
{
    Object a;
    Object b;
    Object c;

    CHECK(a.get_instance_id() != b.get_instance_id());
    CHECK(b.get_instance_id() != c.get_instance_id());
    CHECK(a.get_instance_id() != c.get_instance_id());
}

TEST_CASE("Instance ID: IDs are monotonically increasing")
{
    Object a;
    Object b;
    CHECK(b.get_instance_id() > a.get_instance_id());
}

// ===========================================================================
// Object basics
// ===========================================================================
TEST_CASE("Object: not copyable")
{
    CHECK_FALSE(std::is_copy_constructible_v<Object>);
    CHECK_FALSE(std::is_copy_assignable_v<Object>);
}
