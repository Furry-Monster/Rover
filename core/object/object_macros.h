#pragma once

// ---------------------------------------------------------------------------
// ROVER_CLASS: declares the standard class identity hooks for any subtype of
// rover::Object. Place at the top of the class body. Leaves access set to
// `private:` so subsequent fields default-private.
// ---------------------------------------------------------------------------
#define ROVER_CLASS(m_class, m_parent)                                                                                 \
public:                                                                                                                \
    using ParentClass = m_parent;                                                                                      \
    static rover::StringName get_class_name_static()                                                                   \
    {                                                                                                                  \
        return #m_class;                                                                                               \
    }                                                                                                                  \
    static rover::StringName get_parent_class_name_static()                                                            \
    {                                                                                                                  \
        return #m_parent;                                                                                              \
    }                                                                                                                  \
    rover::StringName get_class_name() const override                                                                  \
    {                                                                                                                  \
        return #m_class;                                                                                               \
    }                                                                                                                  \
    bool is_class(const rover::StringName& p_name) const override                                                      \
    {                                                                                                                  \
        if (p_name == #m_class)                                                                                        \
            return true;                                                                                               \
        return m_parent::is_class(p_name);                                                                             \
    }                                                                                                                  \
                                                                                                                       \
private:

// ---------------------------------------------------------------------------
// ROVER_BIND_PROPERTY: convenience helper for registering a property to the
// owning class via ClassDB. Expects a member field accessible by name and
// derives Variant get/set lambdas using the static_cast pattern below.
//
// Usage (inside a static `bind_methods()` or `register_<T>_types()`):
//
//   ROVER_BIND_PROPERTY(MyType, my_field, PropertyType::Float);
//
// The macro generates a getter that copies the field into a Variant and a
// setter that assigns the Variant back. For complex types provide custom
// getters/setters via `ClassDB::register_property` directly.
// ---------------------------------------------------------------------------
#define ROVER_BIND_PROPERTY(m_class, m_field, m_property_type)                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        rover::PropertyInfo _info;                                                                                     \
        _info.name   = #m_field;                                                                                       \
        _info.type   = (m_property_type);                                                                              \
        _info.getter = [](const rover::Object* obj) {                                                                  \
            const auto* self = static_cast<const m_class*>(obj);                                                       \
            return rover::Variant{self->m_field};                                                                      \
        };                                                                                                             \
        _info.setter = [](rover::Object* obj, const rover::Variant& v) {                                               \
            auto* self    = static_cast<m_class*>(obj);                                                                \
            self->m_field = rover::detail::variant_to<decltype(self->m_field)>(v);                                     \
        };                                                                                                             \
        rover::ClassDB::register_property(#m_class, std::move(_info));                                                 \
    } while (0)
