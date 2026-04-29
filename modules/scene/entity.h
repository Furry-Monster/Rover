#pragma once

#include "modules/scene/world.h"

#include <utility>

namespace rover
{

    // ---------------------------------------------------------------------------
    // Entity: thin handle pairing a World pointer with an entt::entity id.
    //
    // Purely value-based; copying an Entity is cheap. The handle does not own the
    // entity -- you can outlive a destroyed entity, in which case `valid()`
    // returns false and component accessors are no-ops.
    // ---------------------------------------------------------------------------
    class Entity
    {
    public:
        Entity() = default;

        Entity(World* world, World::EntityId id) noexcept : world_(world), id_(id) {}

        [[nodiscard]] bool valid() const noexcept { return world_ != nullptr && world_->valid(id_); }

        [[nodiscard]] World* world() const noexcept { return world_; }

        [[nodiscard]] World::EntityId id() const noexcept { return id_; }

        template <typename T, typename... Args>
        T& add(Args&&... args)
        {
            return world_->add_component<T>(id_, std::forward<Args>(args)...);
        }

        template <typename T>
        [[nodiscard]] T* get()
        {
            return world_ ? world_->get_component<T>(id_) : nullptr;
        }

        template <typename T>
        [[nodiscard]] const T* get() const
        {
            return world_ ? world_->get_component<T>(id_) : nullptr;
        }

        template <typename T>
        [[nodiscard]] bool has() const
        {
            return world_ && world_->has_component<T>(id_);
        }

        template <typename T>
        void remove()
        {
            if (world_)
            {
                world_->remove_component<T>(id_);
            }
        }

        void destroy()
        {
            if (world_)
            {
                world_->destroy_entity(id_);
                world_ = nullptr;
                id_    = entt::null;
            }
        }

        [[nodiscard]] bool operator==(const Entity& rhs) const noexcept
        {
            return world_ == rhs.world_ && id_ == rhs.id_;
        }

        [[nodiscard]] bool operator!=(const Entity& rhs) const noexcept { return !(*this == rhs); }

    private:
        World*          world_ = nullptr;
        World::EntityId id_    = entt::null;
    };

} // namespace rover
