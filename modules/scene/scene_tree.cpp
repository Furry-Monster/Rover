#include "modules/scene/scene_tree.h"

#include "modules/scene/components/hierarchy_component.h"
#include "modules/scene/components/transform_component.h"

#include <algorithm>

namespace rover
{

    void SceneTree::set_parent(World& world, World::EntityId child, World::EntityId parent)
    {
        if (!world.valid(child))
        {
            return;
        }

        // Detach from old parent first so we don't leave dangling pointers.
        unparent(world, child);

        if (parent != entt::null && world.valid(parent))
        {
            auto& pc       = world.add_component<ParentComponent>(child);
            pc.parent      = parent;
            auto& children = world.add_component<ChildrenComponent>(parent);
            children.children.push_back(child);
        }
    }

    void SceneTree::unparent(World& world, World::EntityId child)
    {
        if (!world.valid(child))
        {
            return;
        }
        auto* pc = world.get_component<ParentComponent>(child);
        if (pc == nullptr || pc->parent == entt::null)
        {
            return;
        }

        if (auto* cc = world.get_component<ChildrenComponent>(pc->parent))
        {
            cc->children.erase(std::remove(cc->children.begin(), cc->children.end(), child), cc->children.end());
        }
        pc->parent = entt::null;
        world.remove_component<ParentComponent>(child);
    }

    Mat4 SceneTree::world_matrix(World& world, World::EntityId entity)
    {
        if (!world.valid(entity))
        {
            return Mat4::identity();
        }

        auto* xform = world.get_component<TransformComponent>(entity);
        Mat4  local = xform != nullptr ? xform->to_mat4() : Mat4::identity();

        auto* pc = world.get_component<ParentComponent>(entity);
        if (pc == nullptr || pc->parent == entt::null)
        {
            return local;
        }
        return world_matrix(world, pc->parent) * local;
    }

} // namespace rover
