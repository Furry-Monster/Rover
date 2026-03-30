#include "core/object/class_db.h"

#include "core/object/object.h"

// ---------------------------------------------------------------------------
// Internal storage
// ---------------------------------------------------------------------------

std::unordered_map<StringName, std::unique_ptr<ClassDB::ClassInfo>>&
ClassDB::_get_class_map()
{
    static std::unordered_map<StringName, std::unique_ptr<ClassInfo>> map;
    return map;
}

ClassDB::ClassInfo*
ClassDB::_get_class_info(const StringName& p_class)
{
    auto& map = _get_class_map();
    auto  it  = map.find(p_class);
    if (it == map.end())
        return nullptr;
    return it->second.get();
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void
ClassDB::_add_class(
    const StringName& p_name,
    const StringName& p_parent,
    void*             p_class_ptr)
{
    auto& map = _get_class_map();
    if (map.count(p_name))
        return;

    auto ci        = std::make_unique<ClassInfo>();
    ci->name       = p_name;
    ci->parent_name = p_parent;
    ci->class_ptr  = p_class_ptr;

    if (p_parent)
    {
        auto it = map.find(p_parent);
        if (it != map.end())
            ci->inherits_ptr = it->second.get();
    }

    map.emplace(p_name, std::move(ci));
}

// ---------------------------------------------------------------------------
// Instantiation
// ---------------------------------------------------------------------------

Object*
ClassDB::instantiate(const StringName& p_class)
{
    ClassInfo* ci = _get_class_info(p_class);
    if (!ci || !ci->creation_func)
        return nullptr;
    return ci->creation_func();
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool
ClassDB::class_exists(const StringName& p_class)
{
    return _get_class_info(p_class) != nullptr;
}

ClassDB::ClassInfo*
ClassDB::get_class_info(const StringName& p_class)
{
    return _get_class_info(p_class);
}

bool
ClassDB::is_parent_class(const StringName& p_child, const StringName& p_parent)
{
    if (p_child == p_parent)
        return true;

    ClassInfo* ci = _get_class_info(p_child);
    while (ci)
    {
        if (ci->name == p_parent)
            return true;
        ci = ci->inherits_ptr;
    }
    return false;
}

StringName
ClassDB::get_parent_class(const StringName& p_class)
{
    ClassInfo* ci = _get_class_info(p_class);
    if (!ci)
        return {};
    return ci->parent_name;
}
