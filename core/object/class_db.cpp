#include "core/object/class_db.h"
#include "core/object/object.h"

#include <cassert>

namespace rover {

std::unordered_map<StringName, ClassInfo>& ClassDB::get_registry() {
    static std::unordered_map<StringName, ClassInfo> registry;
    return registry;
}

void ClassDB::register_class(const StringName& name, const StringName& parent,
                              ObjectFactory factory) {
    auto& registry = get_registry();
    assert(!registry.contains(name) && "Class already registered");
    registry[name] = ClassInfo{name, parent, std::move(factory), {}};
}

Object* ClassDB::instantiate(const StringName& class_name) {
    auto& registry = get_registry();
    auto it = registry.find(class_name);
    if (it == registry.end() || !it->second.factory) {
        return nullptr;
    }
    return it->second.factory();
}

bool ClassDB::class_exists(const StringName& name) {
    return get_registry().contains(name);
}

bool ClassDB::is_parent_class(const StringName& child, const StringName& parent) {
    auto& registry = get_registry();
    StringName current = child;
    while (!current.empty()) {
        if (current == parent) return true;
        auto it = registry.find(current);
        if (it == registry.end()) return false;
        current = it->second.parent_name;
    }
    return false;
}

const ClassInfo* ClassDB::get_class_info(const StringName& name) {
    auto& registry = get_registry();
    auto it = registry.find(name);
    if (it == registry.end()) return nullptr;
    return &it->second;
}

} // namespace rover
