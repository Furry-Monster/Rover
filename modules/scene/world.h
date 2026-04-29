#pragma once

#include "core/typedefs.h"

#include <entt/entt.hpp>
#include <string>
#include <utility>

namespace rover
{

    class Entity;

    // ---------------------------------------------------------------------------
    // World: thin wrapper around `entt::registry`.
    //
    // Owns all entities + components for a single ECS scene. Ergonomic helpers
    // mirror EnTT's API (create, destroy, add/get/remove component, view) plus a
    // small `each` convenience over a typed view.
    // ---------------------------------------------------------------------------
    class World
    {
    public:
        using EntityId = entt::entity;

        World()  = default;
        ~World() = default;

        World(const World&)            = delete;
        World& operator=(const World&) = delete;

        // ---- Entity lifecycle ----
        EntityId create_entity();
        void     destroy_entity(EntityId id);

        [[nodiscard]] bool valid(EntityId id) const noexcept { return registry_.valid(id); }

        // ---- Component CRUD ----
        template <typename T, typename... Args>
        T& add_component(EntityId id, Args&&... args)
        {
            return registry_.emplace_or_replace<T>(id, std::forward<Args>(args)...);
        }

        template <typename T>
        [[nodiscard]] T* get_component(EntityId id)
        {
            return registry_.try_get<T>(id);
        }

        template <typename T>
        [[nodiscard]] const T* get_component(EntityId id) const
        {
            return registry_.try_get<T>(id);
        }

        template <typename T>
        [[nodiscard]] bool has_component(EntityId id) const
        {
            return registry_.all_of<T>(id);
        }

        template <typename T>
        void remove_component(EntityId id)
        {
            registry_.remove<T>(id);
        }

        // ---- Queries ----
        template <typename... Components>
        auto view()
        {
            return registry_.view<Components...>();
        }

        template <typename... Components, typename Func>
        void each(Func&& fn)
        {
            auto v = registry_.view<Components...>();
            v.each(std::forward<Func>(fn));
        }

        [[nodiscard]] entt::registry& registry() noexcept { return registry_; }

        [[nodiscard]] const entt::registry& registry() const noexcept { return registry_; }

        [[nodiscard]] usize entity_count() const noexcept { return registry_.storage<EntityId>()->size(); }

        void clear() { registry_.clear(); }

    private:
        entt::registry registry_;
    };

} // namespace rover
