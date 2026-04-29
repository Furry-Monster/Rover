// Unit tests for modules/serialization/.

#include "modules/scene/components/camera_component.h"
#include "modules/scene/components/light_component.h"
#include "modules/scene/components/name_component.h"
#include "modules/scene/components/transform_component.h"
#include "modules/scene/world.h"
#include "modules/serialization/asset_registry.h"
#include "modules/serialization/binary_serializer.h"
#include "modules/serialization/json_deserializer.h"
#include "modules/serialization/json_serializer.h"
#include "modules/serialization/scene_serializer.h"

#include <doctest/doctest.h>

using namespace rover;

// ---------------------------------------------------------------------------
// JSON serializer / deserializer round-trip
// ---------------------------------------------------------------------------
TEST_CASE("JsonSerializer: writes scalar variants in a recognizable shape")
{
    JsonSerializer s;
    s.write_variant(Variant{i64{42}});
    auto out = s.take_output();
    CHECK(out.find("42") != std::string::npos);
}

TEST_CASE("JsonSerializer + JsonDeserializer: dictionary round-trip")
{
    JsonSerializer s;
    VariantDict    d;
    d["name"]  = Variant{std::string{"hero"}};
    d["level"] = Variant{i64{7}};
    d["alive"] = Variant{true};
    d["pos"]   = Variant{Vector3{1.0f, 2.0f, 3.0f}};
    s.write_variant(Variant{std::move(d)});
    auto text = s.take_output();

    JsonDeserializer ds;
    REQUIRE(ds.load(text));
    CHECK(ds.root().type() == Variant::Type::Dictionary);
    const auto& parsed = ds.root().as_dict();
    REQUIRE(parsed.contains("name"));
    CHECK(parsed.at("name").as_string() == "hero");
    CHECK(parsed.at("level").as_int() == 7);
    CHECK(parsed.at("alive").as_bool() == true);
    REQUIRE(parsed.at("pos").type() == Variant::Type::Dictionary);
    CHECK(parsed.at("pos").as_dict().at("x").as_float() == doctest::Approx(1.0));
}

TEST_CASE("JsonSerializer: array of mixed variants round-trips")
{
    JsonSerializer s;
    VariantArray   arr;
    arr.emplace_back(i64{1});
    arr.emplace_back(std::string{"two"});
    arr.emplace_back(true);
    s.write_variant(Variant{std::move(arr)});
    auto text = s.take_output();

    JsonDeserializer ds;
    REQUIRE(ds.load(text));
    REQUIRE(ds.root().type() == Variant::Type::Array);
    const auto& parsed = ds.root().as_array();
    REQUIRE(parsed.size() == 3);
    CHECK(parsed[0].as_int() == 1);
    CHECK(parsed[1].as_string() == "two");
    CHECK(parsed[2].as_bool() == true);
}

// ---------------------------------------------------------------------------
// Binary serializer / deserializer round-trip
// ---------------------------------------------------------------------------
TEST_CASE("BinarySerializer: dict round-trip")
{
    BinarySerializer s;
    VariantDict      d;
    d["int"]    = Variant{i64{-99}};
    d["float"]  = Variant{3.5};
    d["string"] = Variant{std::string{"abc"}};
    d["v3"]     = Variant{Vector3{4.0f, 5.0f, 6.0f}};
    s.write_variant(Variant{std::move(d)});
    auto blob = s.take_output();

    BinaryDeserializer ds;
    REQUIRE(ds.load(blob));
    REQUIRE(ds.root().type() == Variant::Type::Dictionary);
    const auto& parsed = ds.root().as_dict();
    CHECK(parsed.at("int").as_int() == -99);
    CHECK(parsed.at("float").as_float() == doctest::Approx(3.5));
    CHECK(parsed.at("string").as_string() == "abc");
    CHECK(parsed.at("v3").as_vector3() == Vector3{4.0f, 5.0f, 6.0f});
}

TEST_CASE("BinaryDeserializer: rejects bad magic")
{
    BinaryDeserializer ds;
    CHECK_FALSE(ds.load("XBIN\x01\x00\x00\x00"));
}

// ---------------------------------------------------------------------------
// Scene serializer end-to-end
// ---------------------------------------------------------------------------
TEST_CASE("SceneSerializer: round-trip preserves entities + components")
{
    World w;
    auto  cam_ent = w.create_entity();
    w.add_component<NameComponent>(cam_ent, "Camera");
    auto& cam_xform    = w.add_component<TransformComponent>(cam_ent);
    cam_xform.position = Vector3{0.0f, 1.0f, 5.0f};
    auto& cam          = w.add_component<CameraComponent>(cam_ent);
    cam.fov_y_radians  = 1.0f;
    cam.aspect_ratio   = 2.0f;

    auto light_ent = w.create_entity();
    w.add_component<NameComponent>(light_ent, "Sun");
    auto& light     = w.add_component<LightComponent>(light_ent);
    light.type      = LightType::Directional;
    light.color     = Vector3{1.0f, 0.9f, 0.7f};
    light.intensity = 2.5f;

    auto tree = SceneSerializer::to_variant(w);

    World w2;
    REQUIRE(SceneSerializer::from_variant(tree, w2));

    CHECK(w2.entity_count() == 2);

    int found_camera = 0;
    int found_light  = 0;
    w2.each<NameComponent>([&](auto, NameComponent& nc) {
        if (nc.name == "Camera")
        {
            ++found_camera;
        }
        if (nc.name == "Sun")
        {
            ++found_light;
        }
    });
    CHECK(found_camera == 1);
    CHECK(found_light == 1);

    bool checked_cam = false;
    w2.each<NameComponent, CameraComponent, TransformComponent>(
        [&](auto, NameComponent& nc, CameraComponent& cc, TransformComponent& tc) {
            if (nc.name != "Camera")
            {
                return;
            }
            checked_cam = true;
            CHECK(cc.fov_y_radians == doctest::Approx(1.0f));
            CHECK(tc.position == Vector3{0.0f, 1.0f, 5.0f});
        });
    CHECK(checked_cam);
}

// ---------------------------------------------------------------------------
// Asset registry
// ---------------------------------------------------------------------------
TEST_CASE("AssetRegistry: register + find")
{
    AssetRegistry::get().clear();
    auto id = AssetRegistry::get().register_asset(AssetKind::Mesh, "res://meshes/cube.rmesh");
    CHECK(id != INVALID_ASSET_ID);
    CHECK(AssetRegistry::get().find(id)->virtual_path == "res://meshes/cube.rmesh");
    CHECK(AssetRegistry::get().find_id("res://meshes/cube.rmesh") == id);

    // Re-registering the same path should be idempotent.
    auto id2 = AssetRegistry::get().register_asset(AssetKind::Mesh, "res://meshes/cube.rmesh");
    CHECK(id == id2);

    CHECK(AssetRegistry::get().unregister(id));
    CHECK(AssetRegistry::get().find(id) == nullptr);
}
