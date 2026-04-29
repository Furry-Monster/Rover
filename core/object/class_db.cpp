#include "core/object/class_db.h"

#include "core/object/object.h"

#include <cassert>
#include <utility>

namespace rover
{

    std::unordered_map<StringName, ClassInfo>& ClassDB::get_registry()
    {
        static std::unordered_map<StringName, ClassInfo> registry;
        return registry;
    }

    void ClassDB::register_class(const StringName& name, const StringName& parent, ObjectFactory factory)
    {
        auto& registry = get_registry();
        assert(!registry.contains(name) && "Class already registered");
        ClassInfo info{};
        info.name        = name;
        info.parent_name = parent;
        info.factory     = std::move(factory);
        registry.emplace(name, std::move(info));
    }

    bool ClassDB::register_property(const StringName& class_name, PropertyInfo info)
    {
        auto& registry = get_registry();
        auto  it       = registry.find(class_name);
        if (it == registry.end())
        {
            return false;
        }

        auto& cls = it->second;
        auto  pit = cls.property_index.find(info.name);
        if (pit == cls.property_index.end())
        {
            const auto idx = cls.property_order.size();
            cls.property_order.push_back(std::move(info));
            cls.property_index.emplace(cls.property_order[idx].name, idx);
        }
        else
        {
            cls.property_order[pit->second] = std::move(info);
        }
        return true;
    }

    const PropertyInfo* ClassDB::find_property(const StringName& class_name, const StringName& property_name)
    {
        auto& registry = get_registry();
        auto  it       = registry.find(class_name);
        if (it == registry.end())
        {
            return nullptr;
        }
        auto pit = it->second.property_index.find(property_name);
        if (pit == it->second.property_index.end())
        {
            return nullptr;
        }
        return &it->second.property_order[pit->second];
    }

    const std::vector<PropertyInfo>* ClassDB::list_properties(const StringName& class_name)
    {
        auto& registry = get_registry();
        auto  it       = registry.find(class_name);
        if (it == registry.end())
        {
            return nullptr;
        }
        return &it->second.property_order;
    }

    Object* ClassDB::instantiate(const StringName& class_name)
    {
        auto& registry = get_registry();
        auto  it       = registry.find(class_name);
        if (it == registry.end() || !it->second.factory)
        {
            return nullptr;
        }
        return it->second.factory();
    }

    bool ClassDB::class_exists(const StringName& name)
    {
        return get_registry().contains(name);
    }

    bool ClassDB::is_parent_class(const StringName& child, const StringName& parent)
    {
        auto&      registry = get_registry();
        StringName current  = child;
        while (!current.empty())
        {
            if (current == parent)
            {
                return true;
            }
            auto it = registry.find(current);
            if (it == registry.end())
            {
                return false;
            }
            current = it->second.parent_name;
        }
        return false;
    }

    const ClassInfo* ClassDB::get_class_info(const StringName& name)
    {
        auto& registry = get_registry();
        auto  it       = registry.find(name);
        if (it == registry.end())
        {
            return nullptr;
        }
        return &it->second;
    }

} // namespace rover
