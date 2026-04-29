// Unit tests for modules/scene/.

#include "modules/scene/components/camera_component.h"
#include "modules/scene/components/hierarchy_component.h"
#include "modules/scene/components/light_component.h"
#include "modules/scene/components/mesh_component.h"
#include "modules/scene/components/name_component.h"
#include "modules/scene/components/transform_component.h"
#include "modules/scene/entity.h"
#include "modules/scene/scene_tree.h"
#include "modules/scene/world.h"

#include <doctest/doctest.h>

using namespace rover;

TEST_CASE("World: create_entity / destroy_entity round-trip")
{
    World w;
    auto  id = w.create_entity();
    CHECK(w.valid(id));
    w.destroy_entity(id);
    CHECK_FALSE(w.valid(id));
}

TEST_CASE("World: add/get/has/remove component")
{
    World w;
    auto  id = w.create_entity();

    CHECK_FALSE(w.has_component<TransformComponent>(id));
    auto& xform    = w.add_component<TransformComponent>(id);
    xform.position = Vector3{1.0f, 2.0f, 3.0f};

    CHECK(w.has_component<TransformComponent>(id));
    auto* got = w.get_component<TransformComponent>(id);
    REQUIRE(got != nullptr);
    CHECK(got->position == Vector3{1.0f, 2.0f, 3.0f});

    w.remove_component<TransformComponent>(id);
    CHECK_FALSE(w.has_component<TransformComponent>(id));
}

TEST_CASE("World: view iterates only matching entities")
{
    World w;
    auto  a = w.create_entity();
    auto  b = w.create_entity();
    auto  c = w.create_entity();
    w.add_component<TransformComponent>(a);
    w.add_component<TransformComponent>(b);
    w.add_component<MeshComponent>(b);
    w.add_component<MeshComponent>(c);

    int both_count = 0;
    w.each<TransformComponent, MeshComponent>([&](auto, TransformComponent&, MeshComponent&) { ++both_count; });
    CHECK(both_count == 1);

    int  xform_count = 0;
    auto xform_view  = w.view<TransformComponent>();
    for ([[maybe_unused]] auto e : xform_view)
    {
        ++xform_count;
    }
    CHECK(xform_count == 2);
}

TEST_CASE("Entity: handle wraps World CRUD")
{
    World  w;
    Entity e{&w, w.create_entity()};
    CHECK(e.valid());

    e.add<NameComponent>("hero");
    REQUIRE(e.has<NameComponent>());
    CHECK(e.get<NameComponent>()->name == "hero");

    e.destroy();
    CHECK_FALSE(e.valid());
}

TEST_CASE("CameraComponent: produces valid projection matrices")
{
    CameraComponent cam{};
    cam.aspect_ratio = 1.0f;
    auto persp       = cam.projection_matrix();
    CHECK(persp != Mat4{}); // not zero

    cam.projection = CameraProjection::Orthographic;
    auto ortho     = cam.projection_matrix();
    CHECK(ortho != Mat4{});
}

TEST_CASE("SceneTree: parent / unparent updates both ends of the link")
{
    World w;
    auto  parent = w.create_entity();
    auto  child  = w.create_entity();

    SceneTree::set_parent(w, child, parent);
    REQUIRE(w.has_component<ParentComponent>(child));
    CHECK(w.get_component<ParentComponent>(child)->parent == parent);
    auto* children = w.get_component<ChildrenComponent>(parent);
    REQUIRE(children != nullptr);
    REQUIRE(children->children.size() == 1);
    CHECK(children->children[0] == child);

    SceneTree::unparent(w, child);
    CHECK_FALSE(w.has_component<ParentComponent>(child));
    children = w.get_component<ChildrenComponent>(parent);
    REQUIRE(children != nullptr);
    CHECK(children->children.empty());
}

TEST_CASE("SceneTree: world_matrix composes parent transforms")
{
    World w;
    auto  root           = w.create_entity();
    auto  child          = w.create_entity();
    auto& root_xform     = w.add_component<TransformComponent>(root);
    auto& child_xform    = w.add_component<TransformComponent>(child);
    root_xform.position  = Vector3{10.0f, 0.0f, 0.0f};
    child_xform.position = Vector3{0.0f, 5.0f, 0.0f};
    SceneTree::set_parent(w, child, root);

    Mat4 world = SceneTree::world_matrix(w, child);
    // Child's world position should be the sum of the two translations.
    Vector4 origin = world * Vector4{0.0f, 0.0f, 0.0f, 1.0f};
    CHECK(origin.x() == doctest::Approx(10.0f));
    CHECK(origin.y() == doctest::Approx(5.0f));
    CHECK(origin.z() == doctest::Approx(0.0f));
}
