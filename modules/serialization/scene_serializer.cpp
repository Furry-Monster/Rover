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

#include <unordered_map>

namespace rover
{

    namespace
    {

        // Helpers to convert built-in components <-> Variant dicts. Each component
        // has a stable string key used both as the tag in the entity dict and as
        // the dispatch key in `apply_component`.

        Variant transform_to_variant(const TransformComponent& t)
        {
            VariantDict d;
            d["position"] = Variant{t.position};
            d["rotation"] = Variant{t.rotation};
            d["scale"]    = Variant{t.scale};
            return Variant{std::move(d)};
        }

        void variant_to_transform(const Variant& v, TransformComponent& out)
        {
            if (v.type() != Variant::Type::Dictionary)
            {
                return;
            }
            const auto& d = v.as_dict();
            if (auto it = d.find("position"); it != d.end())
            {
                out.position = it->second.as_vector3();
            }
            if (auto it = d.find("rotation"); it != d.end())
            {
                out.rotation = it->second.as_quat();
            }
            if (auto it = d.find("scale"); it != d.end())
            {
                out.scale = it->second.as_vector3();
            }
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

        void variant_to_light(const Variant& v, LightComponent& out)
        {
            if (v.type() != Variant::Type::Dictionary)
            {
                return;
            }
            const auto& d = v.as_dict();
            if (auto it = d.find("type"); it != d.end())
            {
                out.type = static_cast<LightType>(it->second.as_int());
            }
            if (auto it = d.find("color"); it != d.end())
            {
                out.color = it->second.as_vector3();
            }
            if (auto it = d.find("intensity"); it != d.end())
            {
                out.intensity = static_cast<f32>(it->second.as_float());
            }
            if (auto it = d.find("direction"); it != d.end())
            {
                out.direction = it->second.as_vector3();
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
        }

        Variant mesh_to_variant(const MeshComponent& m)
        {
            VariantDict d;
            // Phase 2 stores raw GPU handles; the asset-id path replaces them in
            // Phase 3. For now they round-trip but a fresh load won't have valid
            // GPU resources -- the loader (Sprint 2.6) is expected to re-attach.
            d["vertex_buffer"]  = Variant{static_cast<i64>(m.vertex_buffer)};
            d["index_buffer"]   = Variant{static_cast<i64>(m.index_buffer)};
            d["vertex_count"]   = Variant{static_cast<i64>(m.vertex_count)};
            d["index_count"]    = Variant{static_cast<i64>(m.index_count)};
            d["index_type"]     = Variant{static_cast<i64>(m.index_type)};
            d["albedo_texture"] = Variant{static_cast<i64>(m.albedo_texture)};
            d["albedo_sampler"] = Variant{static_cast<i64>(m.albedo_sampler)};
            return Variant{std::move(d)};
        }

        void variant_to_mesh(const Variant& v, MeshComponent& out)
        {
            if (v.type() != Variant::Type::Dictionary)
            {
                return;
            }
            const auto& d = v.as_dict();
            if (auto it = d.find("vertex_buffer"); it != d.end())
            {
                out.vertex_buffer = static_cast<BufferHandle>(it->second.as_int());
            }
            if (auto it = d.find("index_buffer"); it != d.end())
            {
                out.index_buffer = static_cast<BufferHandle>(it->second.as_int());
            }
            if (auto it = d.find("vertex_count"); it != d.end())
            {
                out.vertex_count = static_cast<u32>(it->second.as_int());
            }
            if (auto it = d.find("index_count"); it != d.end())
            {
                out.index_count = static_cast<u32>(it->second.as_int());
            }
            if (auto it = d.find("index_type"); it != d.end())
            {
                out.index_type = static_cast<IndexType>(it->second.as_int());
            }
            if (auto it = d.find("albedo_texture"); it != d.end())
            {
                out.albedo_texture = static_cast<TextureHandle>(it->second.as_int());
            }
            if (auto it = d.find("albedo_sampler"); it != d.end())
            {
                out.albedo_sampler = static_cast<SamplerHandle>(it->second.as_int());
            }
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
        root["version"]  = Variant{i64{1}};
        root["entities"] = Variant{std::move(entities_arr)};
        return Variant{std::move(root)};
    }

    bool SceneSerializer::from_variant(const Variant& tree, World& out)
    {
        if (tree.type() != Variant::Type::Dictionary)
        {
            return false;
        }
        const auto& root = tree.as_dict();
        auto        it   = root.find("entities");
        if (it == root.end() || it->second.type() != Variant::Type::Array)
        {
            return false;
        }
        const auto& entities = it->second.as_array();

        // Pass 1: create all entities so parent links can resolve.
        std::vector<World::EntityId> created;
        created.reserve(entities.size());
        for (const auto& e : entities)
        {
            World::EntityId id = out.create_entity();
            created.push_back(id);
            if (e.type() != Variant::Type::Dictionary)
            {
                continue;
            }
        }

        // Pass 2: populate components.
        for (usize i = 0; i < entities.size(); ++i)
        {
            const auto& e = entities[i];
            if (e.type() != Variant::Type::Dictionary)
            {
                continue;
            }
            const auto& comps_v = e.as_dict();
            auto        cit     = comps_v.find("components");
            if (cit == comps_v.end() || cit->second.type() != Variant::Type::Dictionary)
            {
                continue;
            }
            const auto& comps = cit->second.as_dict();
            const auto  eid   = created[i];

            if (auto nit = comps.find("name"); nit != comps.end())
            {
                const auto& d   = nit->second.as_dict();
                auto        vit = d.find("value");
                if (vit != d.end())
                {
                    out.add_component<NameComponent>(eid, vit->second.as_string());
                }
            }
            if (auto tit = comps.find("transform"); tit != comps.end())
            {
                auto& tc = out.add_component<TransformComponent>(eid);
                variant_to_transform(tit->second, tc);
            }
            if (auto cit2 = comps.find("camera"); cit2 != comps.end())
            {
                auto& cc = out.add_component<CameraComponent>(eid);
                variant_to_camera(cit2->second, cc);
            }
            if (auto lit = comps.find("light"); lit != comps.end())
            {
                auto& lc = out.add_component<LightComponent>(eid);
                variant_to_light(lit->second, lc);
            }
            if (auto mit = comps.find("mesh"); mit != comps.end())
            {
                auto& mc = out.add_component<MeshComponent>(eid);
                variant_to_mesh(mit->second, mc);
            }
            if (auto pit = comps.find("parent"); pit != comps.end() && pit->second.type() == Variant::Type::Dictionary)
            {
                const auto& d      = pit->second.as_dict();
                auto        idx_it = d.find("index");
                if (idx_it != d.end())
                {
                    const i64 idx = idx_it->second.as_int();
                    if (idx >= 0 && static_cast<usize>(idx) < created.size())
                    {
                        auto& pc       = out.add_component<ParentComponent>(eid);
                        pc.parent      = created[idx];
                        auto& children = out.add_component<ChildrenComponent>(created[idx]);
                        children.children.push_back(eid);
                    }
                }
            }
        }
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
