#pragma once

#include "core/object/property_info.h"
#include "core/variant/variant.h"
#include "core/variant/variant_convert.h"

#include <functional>
#include <unordered_map>
#include <vector>

namespace rover
{

    class Object;

    using ObjectFactory = std::function<Object*()>;

    struct ClassInfo
    {
        StringName    name;
        StringName    parent_name;
        ObjectFactory factory;
        // Insertion-ordered list of properties for stable serialization output.
        std::vector<PropertyInfo>                   property_order;
        std::unordered_map<StringName, std::size_t> property_index;
    };

    class ClassDB
    {
    public:
        ClassDB() = delete;

        static void register_class(const StringName& name, const StringName& parent, ObjectFactory factory);

        template <typename T>
        static void register_class()
        {
            register_class(
                T::get_class_name_static(), T::get_parent_class_name_static(), []() -> Object* { return new T(); });
        }

        // ---- Property registration ----
        // Adds a property to the class. If a property of the same name exists it
        // is replaced. Returns true on success, false if the class is unknown.
        static bool register_property(const StringName& class_name, PropertyInfo info);

        // Lookup helpers
        static const PropertyInfo* find_property(const StringName& class_name, const StringName& property_name);

        // Iterates all properties of `class_name` (own properties only, not parents).
        // Inherited properties are not auto-aggregated to keep ownership clear.
        static const std::vector<PropertyInfo>* list_properties(const StringName& class_name);

        static Object*          instantiate(const StringName& class_name);
        static bool             class_exists(const StringName& name);
        static bool             is_parent_class(const StringName& child, const StringName& parent);
        static const ClassInfo* get_class_info(const StringName& name);

    private:
        static std::unordered_map<StringName, ClassInfo>& get_registry();
    };

} // namespace rover
