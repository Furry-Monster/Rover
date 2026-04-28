#pragma once

#include "core/object/property_info.h"

#include <functional>
#include <unordered_map>

namespace rover {

class Object;

using ObjectFactory = std::function<Object*()>;

struct ClassInfo {
    StringName name;
    StringName parent_name;
    ObjectFactory factory;
    std::unordered_map<StringName, PropertyInfo> properties;
};

class ClassDB {
public:
    ClassDB() = delete;

    static void register_class(const StringName& name, const StringName& parent,
                               ObjectFactory factory);

    template<typename T>
    static void register_class() {
        register_class(
            T::get_class_name_static(),
            T::get_parent_class_name_static(),
            []() -> Object* { return new T(); }
        );
    }

    static Object* instantiate(const StringName& class_name);
    static bool class_exists(const StringName& name);
    static bool is_parent_class(const StringName& child, const StringName& parent);
    static const ClassInfo* get_class_info(const StringName& name);

private:
    static std::unordered_map<StringName, ClassInfo>& get_registry();
};

} // namespace rover
