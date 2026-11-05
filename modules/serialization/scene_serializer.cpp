#include "modules/serialization/scene_serializer.h"

#include "core/log/log.h"
#include "core/os/file_access.h"
#include "modules/scene/components/camera_component.h"
#include "modules/scene/components/hierarchy_component.h"
#include "modules/scene/components/light_component.h"
#include "modules/scene/components/mesh_component.h"
#include "modules/scene/components/name_component.h"
#include "modules/scene/components/transform_component.h"
#include "modules/serialization/binary_serializer.h"
#include "modules/serialization/json_deserializer.h"
#include "modules/serialization/json_serializer.h"

#include <cmath>
#include <unordered_map>
#include <utility>

namespace rover
{

    namespace
    {

        // JSON represents math values as named fields; RBIN keeps typed Variants.
        bool read_math_fields(const Variant& value, const char* const* names, f32* fields, usize count)
        {
            if (value.type() != Variant::Type::Dictionary)
            {
                return false;
            }
            const auto& dict = value.as_dict();
            for (usize i = 0; i < count; ++i)
            {
                auto it = dict.find(names[i]);
                if (it == dict.end() ||
                    (it->second.type() != Variant::Type::Int && it->second.type() != Variant::Type::Float))
                {
                    return false;
                }
                fields[i] = static_cast<f32>(it->second.as_float());
                if (!std::isfinite(fields[i]))
                {
                    return false;
                }
            }
            return true;
        }

        bool read_vector3(const Variant& value, Vector3& out)
        {
            if (value.type() == Variant::Type::Vector3)
            {
                out = value.as_vector3();
                return true;
            }
            const char* names[] = {"x", "y", "z"};
            f32         fields[3]{};
            if (!read_math_fields(value, names, fields, 3))
            {
                return false;
            }
            out = Vector3{fields[0], fields[1], fields[2]};
            return true;
        }

        bool read_quat(const Variant& value, Quat& out)
        {
            if (value.type() == Variant::Type::Quat)
            {
                out = value.as_quat();
                return true;
            }
            const char* names[] = {"w", "x", "y", "z"};
            f32         fields[4]{};
            if (!read_math_fields(value, names, fields, 4))
            {
                return false;
            }
            out = Quat{fields[0], fields[1], fields[2], fields[3]};
            return true;
        }

        Variant transform_to_variant(const TransformComponent& t)
        {
            VariantDict d;
            d["position"] = Variant{t.position};
            d["rotation"] = Variant{t.rotation};
            d["scale"]    = Variant{t.scale};
            return Variant{std::move(d)};
        }

        bool variant_to_transform(const Variant& v, TransformComponent& out)
        {
            if (v.type() != Variant::Type::Dictionary)
            {
                return false;
            }
            const auto& d = v.as_dict();
            if (auto it = d.find("position"); it != d.end())
            {
                if (!read_vector3(it->second, out.position))
                {
                    return false;
                }
            }
            if (auto it = d.find("rotation"); it != d.end())
            {
                if (!read_quat(it->second, out.rotation))
                {
                    return false;
                }
            }
            if (auto it = d.find("scale"); it != d.end())
            {
                if (!read_vector3(it->second, out.scale))
                {
                    return false;
                }
            }
            return true;
        }

        Variant camera_to_variant(const CameraComponent& c)
        {
            VariantDict d;
            d["projection"]    = Variant{static_cast<i64>(c.projection)};
            d["fov_y_radians"] = Variant{static_cast<f64>(c.fov_y_radians)};
            d["near_plane"]    = Variant{static_cast<f64>(c.near_plane)};
            d["far_plane"]     = Variant{static_cast<f64>(c.far_plane)};
            d["ortho_size"]    = Variant{static_cast<f64>(c.ortho_size)};
            d["aspect_ratio"]  = Variant{static_cast<f64>(c.aspect_ratio)};
            d["primary"]       = Variant{c.primary};
            return Variant{std::move(d)};
        }

        void variant_to_camera(const Variant& v, CameraComponent& out)
        {
            if (v.type() != Variant::Type::Dictionary)
            {
                return;
            }
            const auto& d = v.as_dict();
            if (auto it = d.find("projection"); it != d.end())
            {
                out.projection = static_cast<CameraProjection>(it->second.as_int());
            }
            if (auto it = d.find("fov_y_radians"); it != d.end())
            {
                out.fov_y_radians = static_cast<f32>(it->second.as_float());
            }
            if (auto it = d.find("near_plane"); it != d.end())
            {
                out.near_plane = static_cast<f32>(it->second.as_float());
            }
            if (auto it = d.find("far_plane"); it != d.end())
            {
                out.far_plane = static_cast<f32>(it->second.as_float());
            }
            if (auto it = d.find("ortho_size"); it != d.end())
            {
                out.ortho_size = static_cast<f32>(it->second.as_float());
            }
            if (auto it = d.find("aspect_ratio"); it != d.end())
            {
                out.aspect_ratio = static_cast<f32>(it->second.as_float());
            }
            if (auto it = d.find("primary"); it != d.end())
            {
                out.primary = it->second.as_bool();
            }
        }

        Variant light_to_variant(const LightComponent& l)
        {
            VariantDict d;
            d["type"]                = Variant{static_cast<i64>(l.type)};
            d["color"]               = Variant{l.color};
            d["intensity"]           = Variant{static_cast<f64>(l.intensity)};
            d["direction"]           = Variant{l.direction};
            d["range"]               = Variant{static_cast<f64>(l.range)};
            d["spot_cone_inner_cos"] = Variant{static_cast<f64>(l.spot_cone_inner_cos)};
            d["spot_cone_outer_cos"] = Variant{static_cast<f64>(l.spot_cone_outer_cos)};
            return Variant{std::move(d)};
        }

        bool variant_to_light(const Variant& v, LightComponent& out)
        {
            if (v.type() != Variant::Type::Dictionary)
            {
                return false;
            }
            const auto& d = v.as_dict();
            if (auto it = d.find("type"); it != d.end())
            {
                out.type = static_cast<LightType>(it->second.as_int());
            }
            if (auto it = d.find("color"); it != d.end())
            {
                if (!read_vector3(it->second, out.color))
                {
                    return false;
                }
            }
            if (auto it = d.find("intensity"); it != d.end())
            {
                out.intensity = static_cast<f32>(it->second.as_float());
            }
            if (auto it = d.find("direction"); it != d.end())
            {
                if (!read_vector3(it->second, out.direction))
                {
                    return false;
                }
            }
            if (auto it = d.find("range"); it != d.end())
            {
                out.range = static_cast<f32>(it->second.as_float());
            }
            if (auto it = d.find("spot_cone_inner_cos"); it != d.end())
            {
                out.spot_cone_inner_cos = static_cast<f32>(it->second.as_float());
            }
            if (auto it = d.find("spot_cone_outer_cos"); it != d.end())
            {
                out.spot_cone_outer_cos = static_cast<f32>(it->second.as_float());
            }
            return true;
        }

        Variant mesh_to_variant(const MeshComponent& mesh)
        {
            VariantDict dict;
            dict["mesh_asset"] = Variant{mesh.mesh_asset};
            return Variant{std::move(dict)};
        }

        bool variant_to_mesh(const Variant& value, MeshComponent& out)
        {
            const auto& dict = value.as_dict();
            if (auto it = dict.find("mesh_asset"); it != dict.end())
            {
                if (it->second.type() != Variant::Type::String)
                {
                    return false;
                }
                out.mesh_asset = it->second.as_string();
            }
            // Legacy GPU handles are process-local and must never be restored.
            return true;
        }

    } // namespace

    Variant SceneSerializer::to_variant(World& world)
    {
        VariantArray entities_arr;

        // Collect all live entities. Using view<entt::entity> in EnTT 4 returns
        // a view over the entity storage which is iterable.
        auto&                        reg = world.registry();
        std::vector<World::EntityId> all_entities;
        auto                         entity_view = reg.view<entt::entity>();
        for (auto e : entity_view)
        {
            all_entities.push_back(e);
        }

        // Map entt::entity -> sequential index so parent links survive a load
        // even though entt::entity ids are non-monotonic.
        std::unordered_map<World::EntityId, i64> id_to_index;
        for (i64 i = 0; i < static_cast<i64>(all_entities.size()); ++i)
        {
            id_to_index[all_entities[i]] = i;
        }

        for (auto e : all_entities)
        {
            VariantDict entity_dict;
            entity_dict["id"] = Variant{id_to_index[e]};

            VariantDict comps;

            if (auto* nc = world.get_component<NameComponent>(e))
            {
                VariantDict name_d;
                name_d["value"] = Variant{nc->name};
                comps["name"]   = Variant{std::move(name_d)};
            }
            if (auto* t = world.get_component<TransformComponent>(e))
            {
                comps["transform"] = transform_to_variant(*t);
            }
            if (auto* c = world.get_component<CameraComponent>(e))
            {
                comps["camera"] = camera_to_variant(*c);
            }
            if (auto* l = world.get_component<LightComponent>(e))
            {
                comps["light"] = light_to_variant(*l);
            }
            if (auto* m = world.get_component<MeshComponent>(e))
            {
                comps["mesh"] = mesh_to_variant(*m);
            }
            if (auto* p = world.get_component<ParentComponent>(e))
            {
                if (p->parent != entt::null && id_to_index.contains(p->parent))
                {
                    VariantDict pd;
                    pd["index"]     = Variant{id_to_index[p->parent]};
                    comps["parent"] = Variant{std::move(pd)};
                }
            }

            entity_dict["components"] = Variant{std::move(comps)};
            entities_arr.push_back(Variant{std::move(entity_dict)});
        }

        VariantDict root;
        root["version"]  = Variant{i64{2}};
        root["entities"] = Variant{std::move(entities_arr)};
        return Variant{std::move(root)};
    }

    bool SceneSerializer::from_variant(const Variant& tree, World& out)
    {
        if (tree.type() != Variant::Type::Dictionary)
        {
            ROVER_LOG_ERROR("SceneSerializer: expected scene dictionary");
            return false;
        }
        const auto& root = tree.as_dict();
        if (auto version = root.find("version"); version != root.end())
        {
            if (version->second.type() != Variant::Type::Int || version->second.as_int() < 1 ||
                version->second.as_int() > 2)
            {
                ROVER_LOG_ERROR("SceneSerializer: unsupported scene version");
                return false;
            }
        }
        auto it = root.find("entities");
        if (it == root.end() || it->second.type() != Variant::Type::Array)
        {
            ROVER_LOG_ERROR("SceneSerializer: expected entities array");
            return false;
        }
        const auto& entities = it->second.as_array();

        World                        loaded;
        std::vector<World::EntityId> created;
        std::vector<i64>             parents(entities.size(), -1);
        created.reserve(entities.size());
        for (usize i = 0; i < entities.size(); ++i)
        {
            created.push_back(loaded.create_entity());
        }

        for (usize i = 0; i < entities.size(); ++i)
        {
            const auto& entity = entities[i];
            if (entity.type() != Variant::Type::Dictionary)
            {
                ROVER_LOG_ERROR("SceneSerializer: entity {} must be a dictionary", i);
                return false;
            }
            auto components = entity.as_dict().find("components");
            if (components == entity.as_dict().end())
            {
                continue;
            }
            if (components->second.type() != Variant::Type::Dictionary)
            {
                ROVER_LOG_ERROR("SceneSerializer: entity {} components must be a dictionary", i);
                return false;
            }
            const auto& comps = components->second.as_dict();
            const auto  eid   = created[i];
            for (const char* key : {"name", "transform", "camera", "light", "mesh", "parent"})
            {
                if (auto component = comps.find(key);
                    component != comps.end() && component->second.type() != Variant::Type::Dictionary)
                {
                    ROVER_LOG_ERROR("SceneSerializer: invalid {} component on entity {}", key, i);
                    return false;
                }
            }
            if (auto name = comps.find("name"); name != comps.end())
            {
                const auto& dict = name->second.as_dict();
                if (auto value = dict.find("value"); value != dict.end())
                {
                    if (value->second.type() != Variant::Type::String)
                    {
                        ROVER_LOG_ERROR("SceneSerializer: name must be a string");
                        return false;
                    }
                    loaded.add_component<NameComponent>(eid, value->second.as_string());
                }
            }
            if (auto transform = comps.find("transform"); transform != comps.end())
            {
                if (!variant_to_transform(transform->second, loaded.add_component<TransformComponent>(eid)))
                {
                    ROVER_LOG_ERROR("SceneSerializer: invalid transform on entity {}", i);
                    return false;
                }
            }
            if (auto camera = comps.find("camera"); camera != comps.end())
            {
                variant_to_camera(camera->second, loaded.add_component<CameraComponent>(eid));
            }
            if (auto light = comps.find("light"); light != comps.end())
            {
                if (!variant_to_light(light->second, loaded.add_component<LightComponent>(eid)))
                {
                    ROVER_LOG_ERROR("SceneSerializer: invalid light on entity {}", i);
                    return false;
                }
            }
            if (auto mesh = comps.find("mesh"); mesh != comps.end())
            {
                if (!variant_to_mesh(mesh->second, loaded.add_component<MeshComponent>(eid)))
                {
                    ROVER_LOG_ERROR("SceneSerializer: mesh_asset must be a string on entity {}", i);
                    return false;
                }
            }
            if (auto parent = comps.find("parent"); parent != comps.end())
            {
                const auto& dict  = parent->second.as_dict();
                auto        index = dict.find("index");
                if (index == dict.end() || index->second.type() != Variant::Type::Int || index->second.as_int() < 0 ||
                    static_cast<usize>(index->second.as_int()) >= entities.size())
                {
                    ROVER_LOG_ERROR("SceneSerializer: invalid parent index on entity {}", i);
                    return false;
                }
                parents[i] = index->second.as_int();
            }
        }

        // Validate parent chains before exposing them to recursive world transforms.
        std::vector<u8> visited(entities.size(), 0);
        for (usize i = 0; i < parents.size(); ++i)
        {
            i64 current = static_cast<i64>(i);
            while (current >= 0 && visited[current] == 0)
            {
                visited[current] = 1;
                current          = parents[current];
            }
            if (current >= 0 && visited[current] == 1)
            {
                ROVER_LOG_ERROR("SceneSerializer: cyclic parent hierarchy");
                return false;
            }
            current = static_cast<i64>(i);
            while (current >= 0 && visited[current] == 1)
            {
                visited[current] = 2;
                current          = parents[current];
            }
        }
        for (usize i = 0; i < parents.size(); ++i)
        {
            if (parents[i] < 0)
            {
                continue;
            }
            const auto parent                                        = created[parents[i]];
            loaded.add_component<ParentComponent>(created[i]).parent = parent;
            auto* children                                           = loaded.get_component<ChildrenComponent>(parent);
            if (children == nullptr)
            {
                children = &loaded.add_component<ChildrenComponent>(parent);
            }
            children->children.push_back(created[i]);
        }

        out.registry() = std::move(loaded.registry());
        return true;
    }

    void SceneSerializer::serialize(World& world, Serializer& out)
    {
        auto root = to_variant(world);
        out.write_variant(root);
    }

    bool SceneSerializer::deserialize(const Deserializer& in, World& out)
    {
        return from_variant(in.root(), out);
    }

    bool SceneSerializer::save_json(World& world, const std::string& path)
    {
        JsonSerializer js;
        serialize(world, js);
        return FileAccess::write_text_file(path, js.take_output());
    }

    bool SceneSerializer::load_json(const std::string& path, World& out)
    {
        auto text = FileAccess::read_text_file(path);
        if (text.empty())
        {
            ROVER_LOG_WARN("SceneSerializer: load_json failed to read '{}'", path);
            return false;
        }
        JsonDeserializer js;
        if (!js.load(text))
        {
            ROVER_LOG_ERROR("SceneSerializer: invalid JSON in '{}'", path);
            return false;
        }
        return from_variant(js.root(), out);
    }

    bool SceneSerializer::save_binary(World& world, const std::string& path)
    {
        BinarySerializer bs;
        serialize(world, bs);
        return FileAccess::write_text_file(path, bs.take_output());
    }

    bool SceneSerializer::load_binary(const std::string& path, World& out)
    {
        auto data = FileAccess::read_text_file(path);
        if (data.empty())
        {
            ROVER_LOG_WARN("SceneSerializer: load_binary failed to read '{}'", path);
            return false;
        }
        BinaryDeserializer bs;
        if (!bs.load(data))
        {
            ROVER_LOG_ERROR("SceneSerializer: invalid binary in '{}'", path);
            return false;
        }
        return from_variant(bs.root(), out);
    }

} // namespace rover
