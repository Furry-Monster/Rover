#include "core/os/file_access.h"
#include "modules/scene/components/camera_component.h"
#include "modules/scene/components/hierarchy_component.h"
#include "modules/scene/components/light_component.h"
#include "modules/scene/components/name_component.h"
#include "modules/scene/components/transform_component.h"
#include "modules/scene/scene_tree.h"
#include "modules/serialization/json_deserializer.h"
#include "modules/serialization/mesh_asset_cache.h"
#include "modules/serialization/scene_serializer.h"
#include "platform/linux/file_access_linux.h"
#include "tests/mocks/mock_graphics_device.h"

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

using namespace rover;

namespace
{

    struct TempSceneDirectory
    {
        std::string path;

        TempSceneDirectory()
        {
            FileAccess::set_factory(&create_file_access_linux);
            auto pattern = (std::filesystem::temp_directory_path() / "rover_scene_XXXXXX").string();
            if (auto* directory = mkdtemp(pattern.data()))
            {
                path = directory;
            }
        }

        ~TempSceneDirectory()
        {
            if (!path.empty())
            {
                std::error_code error;
                std::filesystem::remove_all(path, error);
            }
        }
    };

    class BufferDevice : public MockGraphicsDevice
    {
    public:
        u64                       next_handle        = 100;
        u32                       allocations        = 0;
        u32                       fail_on_allocation = 0;
        u32                       uploads            = 0;
        u32                       idle_waits         = 0;
        std::vector<BufferHandle> destroyed;

        BufferHandle create_buffer(const BufferDesc&) override
        {
            ++allocations;
            return allocations == fail_on_allocation ? INVALID_HANDLE : next_handle++;
        }

        void update_buffer(BufferHandle, const void*, usize, usize) override { ++uploads; }

        void destroy_buffer(BufferHandle handle) override
        {
            events.emplace_back("destroy_buffer");
            destroyed.push_back(handle);
        }

        void wait_idle() override
        {
            events.emplace_back("wait_idle");
            ++idle_waits;
        }
    };

    World::EntityId add_mesh(World& world, const std::string& path)
    {
        const auto entity                                     = world.create_entity();
        world.add_component<MeshComponent>(entity).mesh_asset = path;
        return entity;
    }

} // namespace

TEST_CASE("Scene files restore transforms, lights, hierarchy and stable mesh paths")
{
    TempSceneDirectory directory;
    REQUIRE_FALSE(directory.path.empty());
    bool binary = false;
    SUBCASE("JSON") {}
    SUBCASE("binary")
    {
        binary = true;
    }

    const auto path     = directory.path + "/scene.rscene";
    const auto rotation = Quat::from_axis_angle(Vector3{1.0f, 2.0f, 3.0f}, 0.7f);
    {
        World source;
        auto  parent = source.create_entity();
        source.add_component<NameComponent>(parent, "Root");
        source.add_component<TransformComponent>(parent).position = Vector3{5.0f, 0.0f, 0.0f};
        auto camera                                               = source.create_entity();
        source.add_component<NameComponent>(camera, "Camera");
        source.add_component<CameraComponent>(camera).fov_y_radians = 0.9f;
        source.add_component<TransformComponent>(camera).position   = Vector3{0.0f, 1.0f, 5.0f};
        auto  light                                                 = source.create_entity();
        auto& lc                                                    = source.add_component<LightComponent>(light);
        lc.color                                                    = Vector3{0.2f, 0.5f, 0.8f};
        lc.direction                                                = Vector3{-1.0f, -2.0f, -3.0f};
        lc.intensity                                                = 2.5f;
        for (int i = 0; i < 2; ++i)
        {
            auto child                                          = add_mesh(source, "builtin://meshes/cube");
            source.add_component<ParentComponent>(child).parent = parent;
            auto& transform                                     = source.add_component<TransformComponent>(child);
            transform.position                                  = Vector3{1.0f, 2.0f, 3.0f};
            transform.scale                                     = Vector3{2.0f, 3.0f, 4.0f};
            transform.rotation                                  = rotation;
            auto& mesh                                          = *source.get_component<MeshComponent>(child);
            mesh.vertex_buffer                                  = 41;
            mesh.index_buffer                                   = 42;
            mesh.albedo_texture                                 = 43;
            mesh.albedo_sampler                                 = 44;
            mesh.vertex_count                                   = 999;
        }
        REQUIRE((binary ? SceneSerializer::save_binary(source, path) : SceneSerializer::save_json(source, path)));
    }

    World loaded;
    auto  stale = loaded.create_entity();
    loaded.add_component<NameComponent>(stale, "Must be replaced");
    REQUIRE((binary ? SceneSerializer::load_binary(path, loaded) : SceneSerializer::load_json(path, loaded)));
    CHECK(loaded.entity_count() == 5);
    u32 cameras = 0;
    loaded.each<CameraComponent, TransformComponent>([&](auto, CameraComponent& camera, TransformComponent& transform) {
        ++cameras;
        CHECK(camera.fov_y_radians == doctest::Approx(0.9f));
        CHECK(transform.position == Vector3{0.0f, 1.0f, 5.0f});
    });
    CHECK(cameras == 1);
    u32 lights = 0;
    loaded.each<LightComponent>([&](auto, LightComponent& light) {
        ++lights;
        CHECK(light.color == Vector3{0.2f, 0.5f, 0.8f});
        CHECK(light.direction == Vector3{-1.0f, -2.0f, -3.0f});
        CHECK(light.intensity == doctest::Approx(2.5f));
    });
    CHECK(lights == 1);
    u32 meshes = 0;
    loaded.each<MeshComponent, TransformComponent>(
        [&](auto entity, MeshComponent& mesh, TransformComponent& transform) {
            ++meshes;
            CHECK(mesh.mesh_asset == "builtin://meshes/cube");
            CHECK(mesh.vertex_buffer == INVALID_HANDLE);
            CHECK(mesh.index_buffer == INVALID_HANDLE);
            CHECK(mesh.albedo_texture == INVALID_HANDLE);
            CHECK(mesh.albedo_sampler == INVALID_HANDLE);
            CHECK(mesh.vertex_count == 0);
            CHECK(transform.position == Vector3{1.0f, 2.0f, 3.0f});
            CHECK(transform.scale == Vector3{2.0f, 3.0f, 4.0f});
            CHECK(transform.rotation.x() == doctest::Approx(rotation.x()));
            CHECK(transform.rotation.y() == doctest::Approx(rotation.y()));
            CHECK(transform.rotation.z() == doctest::Approx(rotation.z()));
            CHECK(transform.rotation.w() == doctest::Approx(rotation.w()));
            const auto matrix = SceneTree::world_matrix(loaded, entity);
            CHECK(matrix.c[3][0] == doctest::Approx(6.0f));
            CHECK(matrix.c[3][1] == doctest::Approx(2.0f));
        });
    CHECK(meshes == 2);
    u32 parents = 0;
    loaded.each<ChildrenComponent>([&](auto, ChildrenComponent& children) {
        ++parents;
        CHECK(children.children.size() == 2);
    });
    CHECK(parents == 1);

    // Simulate a fresh device: runtime handles differ from the saved process.
    BufferDevice   device;
    MeshAssetCache cache(device);
    REQUIRE(cache.resolve(loaded));
    CHECK(device.allocations == 2);
    loaded.each<MeshComponent>([&](auto, MeshComponent& mesh) {
        CHECK(mesh.vertex_buffer == 100);
        CHECK(mesh.index_buffer == 101);
        CHECK(mesh.index_count == 36);
    });
}

TEST_CASE("Scene serialization never persists runtime GPU resources")
{
    World world;
    auto  entity        = add_mesh(world, "builtin://meshes/sphere");
    auto& mesh          = *world.get_component<MeshComponent>(entity);
    mesh.vertex_buffer  = 10;
    mesh.index_buffer   = 11;
    mesh.albedo_texture = 12;
    mesh.albedo_sampler = 13;
    mesh.vertex_count   = 24;
    mesh.index_count    = 36;
    const auto tree     = SceneSerializer::to_variant(world);
    CHECK(tree.as_dict().at("version").as_int() == 2);
    const auto& components = tree.as_dict().at("entities").as_array()[0].as_dict().at("components").as_dict();
    const auto& saved_mesh = components.at("mesh").as_dict();
    CHECK(saved_mesh.size() == 1);
    CHECK(saved_mesh.at("mesh_asset").as_string() == "builtin://meshes/sphere");
}

TEST_CASE("Legacy scenes discard stale GPU handles")
{
    JsonDeserializer json;
    REQUIRE(json.load(R"({"version":1,"entities":[{"components":{"mesh":{
        "vertex_buffer":987,"index_buffer":988,"vertex_count":24,"index_count":36,
        "albedo_texture":989,"albedo_sampler":990}}}]})"));
    World world;
    REQUIRE(SceneSerializer::deserialize(json, world));
    world.each<MeshComponent>([](auto, MeshComponent& mesh) {
        CHECK(mesh.vertex_buffer == INVALID_HANDLE);
        CHECK(mesh.index_buffer == INVALID_HANDLE);
        CHECK(mesh.albedo_texture == INVALID_HANDLE);
        CHECK(mesh.albedo_sampler == INVALID_HANDLE);
        CHECK(mesh.vertex_count == 0);
        CHECK(mesh.mesh_asset.empty());
    });
    BufferDevice   device;
    MeshAssetCache cache(device);
    CHECK_FALSE(cache.resolve(world));
    CHECK(device.allocations == 0);
}

TEST_CASE("Invalid scene data leaves the existing world intact")
{
    std::string source;
    SUBCASE("unsupported version")
    {
        source = R"({"version":99,"entities":[]})";
    }
    SUBCASE("invalid version type")
    {
        source = R"({"version":"2","entities":[]})";
    }
    SUBCASE("invalid entity")
    {
        source = R"({"version":2,"entities":[0]})";
    }
    SUBCASE("invalid components")
    {
        source = R"({"version":2,"entities":[{"components":[]}]})";
    }
    SUBCASE("invalid component")
    {
        source = R"({"version":2,"entities":[{"components":{"camera":0}}]})";
    }
    SUBCASE("invalid mesh path")
    {
        source = R"({"version":2,"entities":[{"components":{"mesh":{"mesh_asset":12}}}]})";
    }
    SUBCASE("incomplete vector")
    {
        source = R"({"version":2,"entities":[{"components":{"transform":{"position":{"x":1,"y":2}}}}]})";
    }
    SUBCASE("bad parent")
    {
        source = R"({"version":2,"entities":[{"components":{"parent":{"index":5}}}]})";
    }
    SUBCASE("self parent")
    {
        source = R"({"version":2,"entities":[{"components":{"parent":{"index":0}}}]})";
    }
    SUBCASE("parent cycle")
    {
        source = R"({"version":2,"entities":[{"components":{"parent":{"index":1}}},
                   {"components":{"parent":{"index":0}}}]})";
    }
    JsonDeserializer json;
    REQUIRE(json.load(source));
    World      world;
    const auto original = world.create_entity();
    world.add_component<NameComponent>(original, "Keep me");
    CHECK_FALSE(SceneSerializer::deserialize(json, world));
    CHECK(world.entity_count() == 1);
    REQUIRE(world.valid(original));
    REQUIRE(world.has_component<NameComponent>(original));
    CHECK(world.get_component<NameComponent>(original)->name == "Keep me");
}

TEST_CASE("Unreadable or malformed scene files leave the destination unchanged")
{
    TempSceneDirectory directory;
    REQUIRE_FALSE(directory.path.empty());
    const auto path = directory.path + "/invalid.rscene";
    SUBCASE("missing file") {}
    SUBCASE("incomplete JSON")
    {
        REQUIRE(FileAccess::write_text_file(path, "{\"entities\":["));
    }
    SUBCASE("trailing JSON")
    {
        REQUIRE(FileAccess::write_text_file(path, "{\"entities\":[]} garbage"));
    }
    World      world;
    const auto original = world.create_entity();
    CHECK_FALSE(SceneSerializer::load_json(path, world));
    CHECK(world.entity_count() == 1);
    CHECK(world.valid(original));
}

TEST_CASE("Mesh cache shares buffers and releases them once after GPU work completes")
{
    BufferDevice device;
    World        world;
    const auto   cube_a = add_mesh(world, "builtin://meshes/cube");
    const auto   cube_b = add_mesh(world, "builtin://meshes/cube");
    const auto   quad   = add_mesh(world, "builtin://meshes/quad");
    const auto   sphere = add_mesh(world, "builtin://meshes/sphere");
    {
        MeshAssetCache cache(device);
        REQUIRE(cache.resolve(world));
        CHECK(device.allocations == 6);
        CHECK(device.uploads == 6);
        CHECK(world.get_component<MeshComponent>(cube_a)->vertex_buffer ==
              world.get_component<MeshComponent>(cube_b)->vertex_buffer);
        CHECK(world.get_component<MeshComponent>(quad)->index_count == 6);
        CHECK(world.get_component<MeshComponent>(sphere)->index_count > 36);
        REQUIRE(cache.resolve(world));
        CHECK(device.allocations == 6);
    }
    CHECK(device.idle_waits == 1);
    CHECK(device.destroyed.size() == 6);
    REQUIRE(device.events.size() == 7);
    CHECK(device.events.front() == "wait_idle");
    for (usize i = 1; i < device.events.size(); ++i)
    {
        CHECK(device.events[i] == "destroy_buffer");
    }
}

TEST_CASE("Mesh cache rejects unknown references without changing components")
{
    BufferDevice device;
    World        world;
    const auto   missing = add_mesh(world, "res://missing.rmesh");
    const auto   valid   = add_mesh(world, "builtin://meshes/cube");
    // EnTT visits the last entity first, exercising partial cache population.
    world.get_component<MeshComponent>(valid)->vertex_buffer = 77;
    {
        MeshAssetCache cache(device);
        CHECK_FALSE(cache.resolve(world));
        CHECK(world.get_component<MeshComponent>(valid)->vertex_buffer == 77);
        CHECK(world.get_component<MeshComponent>(missing)->vertex_buffer == INVALID_HANDLE);
        CHECK(device.allocations == 2);
    }
    CHECK(device.destroyed.size() == 2);
}

TEST_CASE("Mesh upload failure cleans partial buffers and allows retry")
{
    BufferDevice device;
    device.fail_on_allocation = 2;
    World      world;
    const auto entity = add_mesh(world, "builtin://meshes/cube");
    {
        MeshAssetCache cache(device);
        CHECK_FALSE(cache.resolve(world));
        CHECK(world.get_component<MeshComponent>(entity)->vertex_buffer == INVALID_HANDLE);
        REQUIRE(device.destroyed.size() == 1);
        CHECK(device.destroyed[0] == 100);
        device.fail_on_allocation = 0;
        REQUIRE(cache.resolve(world));
        CHECK(world.get_component<MeshComponent>(entity)->vertex_buffer == 101);
        cache.clear();
        CHECK(device.destroyed.size() == 3);
        REQUIRE(cache.resolve(world));
        CHECK(world.get_component<MeshComponent>(entity)->vertex_buffer == 103);
    }
    CHECK(device.destroyed.size() == 5);
    CHECK(device.idle_waits == 2);
}
