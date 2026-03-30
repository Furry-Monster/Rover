#pragma once

#include "core/os/memory.h"
#include "core/string/string_name.h"

#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

class Object;

/**
 * @brief
 *
 * ClassDB — global registry of all Object-derived classes.
 *
 * Every class that uses ROVER_CLASS is registered here during
 * initialize_class().  register_class<T>() additionally installs a
 * creation function so that instances can be created by name at runtime.
 */
class ClassDB
{
public:
    struct ClassInfo
    {
        StringName name;
        StringName parent_name;
        void*      class_ptr    = nullptr;
        ClassInfo* inherits_ptr = nullptr;

        Object* (*creation_func)() = nullptr;
        bool exposed               = false;

        std::vector<StringName> signal_list;
    };

    // -- Registration -------------------------------------------------------

    template <typename T>
    static void register_class()
    {
        static_assert(std::is_same_v<typename T::self_type, T>, "Class not declared properly. Use ROVER_CLASS.");
        T::initialize_class();

        ClassInfo* ci = _get_class_info(T::get_class_static());
        if (!ci)
        {
            return;
        }

        ci->creation_func = &_creator<T>;
        ci->exposed       = true;
        ci->class_ptr     = T::get_class_ptr_static();
    }

    // -- Instantiation ------------------------------------------------------

    static Object* instantiate(const StringName& p_class);

    // -- Queries ------------------------------------------------------------

    static bool       class_exists(const StringName& p_class);
    static bool       is_parent_class(const StringName& p_child, const StringName& p_parent);
    static StringName get_parent_class(const StringName& p_class);
    static ClassInfo* get_class_info(const StringName& p_class);

    // -- Signal registration (called from _bind_methods) ---------------------

    static void add_signal(const StringName& p_class, const StringName& p_signal);
    static bool has_signal(const StringName& p_class, const StringName& p_signal);

    // -- Internal (called by ROVER_CLASS / Object::initialize_class) --------

    static void _add_class(const StringName& p_name, const StringName& p_parent, void* p_class_ptr);

private:
    template <typename T>
    static Object* _creator()
    {
        return memnew<T>();
    }

    static ClassInfo*                                                  _get_class_info(const StringName& p_class);
    static std::unordered_map<StringName, std::unique_ptr<ClassInfo>>& _get_class_map();
};
