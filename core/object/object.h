#pragma once

#include "core/error/error_list.h"
#include "core/object/callable.h"
#include "core/string/string_name.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

class ClassDB;
class Variant;

/**
 * @brief
 *
 * Standard notifications understood by Object itself.
 * Subsystems (Node, etc.) extend with their own ranges.
 */
enum ObjectNotification
{
    NOTIFICATION_POSTINITIALIZE = 0,
    NOTIFICATION_PREDELETE      = 1,
};

/**
 * @brief
 *
 * Object — root of all engine types that need RTTI, notifications, and
 * ClassDB registration.
 */
class Object
{
public:
    using self_type = Object;

    // -- RTTI ---------------------------------------------------------------

    static void* get_class_ptr_static()
    {
        static int ptr;
        return &ptr;
    }

    virtual bool is_class_ptr(void* p_ptr) const { return p_ptr == get_class_ptr_static(); }

    static const StringName& get_class_static()
    {
        static StringName name("Object");
        return name;
    }

    virtual const StringName& get_class_name() const { return get_class_static(); }

    virtual bool is_class(const StringName& p_class) const { return p_class == get_class_static(); }

    template <typename T>
    static T* cast_to(Object* p_object)
    {
        if (p_object && p_object->is_class_ptr(T::get_class_ptr_static()))
        {
            return static_cast<T*>(p_object);
        }
        return nullptr;
    }

    template <typename T>
    static const T* cast_to(const Object* p_object)
    {
        if (p_object && p_object->is_class_ptr(T::get_class_ptr_static()))
        {
            return static_cast<const T*>(p_object);
        }
        return nullptr;
    }

    // -- Instance identity --------------------------------------------------

    uint64_t get_instance_id() const noexcept { return _instance_id; }

    // -- Notifications ------------------------------------------------------

    void notification(int p_what, bool p_reversed = false);

    // -- ClassDB integration ------------------------------------------------

    static void initialize_class();

    static void _bind_methods() {}

    // -- Signals ------------------------------------------------------------

    Error connect(const StringName& p_signal, const Callable& p_callable);
    Error connect(const StringName& p_signal, Object* p_target, const StringName& p_method);
    void  disconnect(const StringName& p_signal, Object* p_target, const StringName& p_method);
    bool  is_connected(const StringName& p_signal, Object* p_target, const StringName& p_method) const;

    template <typename... Args>
    void emit_signal(const StringName& p_signal, Args&&... args);

    void emit_signal_argv(const StringName& p_signal, const Variant* p_args, int p_arg_count);

    // -- Virtual method call (used by Callable for Object+method) -----------

    virtual Variant call(const StringName& p_method, const Variant* p_args, int p_arg_count);

    // -- Lifecycle ----------------------------------------------------------

    Object();
    virtual ~Object();

    Object(const Object&)            = delete;
    Object& operator=(const Object&) = delete;

protected:
    // Note:
    // Subclasses define their own non-virtual _notification(int) to handle
    // notifications.  The ROVER_CLASS dispatch chain detects overrides via
    // member-function-pointer comparison and calls each level exactly once.
    void _notification(int) {}

    static void (Object::*_get_notification())(int) { return &Object::_notification; }

    virtual void _notification_dispatch_forward(int p_what);
    virtual void _notification_dispatch_backward(int p_what);

    virtual void _initialize_classv() { initialize_class(); }

    // Indirection so that object.h does not need to #include class_db.h.
    static void _add_class_to_classdb(const StringName& p_name, const StringName& p_parent, void* p_ptr);

private:
    uint64_t        _instance_id = 0;
    static uint64_t _next_instance_id;

    struct SignalConnection
    {
        Callable callable;
    };

    std::unordered_map<StringName, std::vector<SignalConnection>> _signal_map;

    friend class ClassDB;
};

// ---------------------------------------------------------------------------
// Variadic emit_signal — defined here so Variant is visible via callable.h
// (Variant only needs forward-decl in the class body above.)
// ---------------------------------------------------------------------------
// Include variant.h so the template body can construct Variant from Args.
#include "core/variant/variant.h"

template <typename... Args>
void
Object::emit_signal(const StringName& p_signal, Args&&... args)
{
    if constexpr (sizeof...(Args) == 0)
    {
        emit_signal_argv(p_signal, nullptr, 0);
    }
    else
    {
        Variant argv[] = {Variant(std::forward<Args>(args))...};
        emit_signal_argv(p_signal, argv, static_cast<int>(sizeof...(Args)));
    }
}

// ---------------------------------------------------------------------------
// ROVER_CLASS(m_class, m_parent)
//
// Place at the very top of every Object-derived class body.  Provides:
//   • RTTI   — is_class / is_class_ptr / cast_to
//   • Name   — get_class_static / get_class_name
//   • ClassDB registration via initialize_class()
//   • Notification dispatch chain (forward & backward)
//
// The user may optionally define:
//   void _notification(int p_what)   – per-class notification handler
//   static void _bind_methods()      – property / signal registration
//                                       (called automatically during init)
// ---------------------------------------------------------------------------
// clang-format off
#define ROVER_CLASS(m_class, m_parent)                                          \
public:                                                                         \
    using self_type  = m_class;                                                 \
    using super_type = m_parent;                                                \
                                                                                \
    static void* get_class_ptr_static()                                         \
    {                                                                           \
        static int ptr;                                                         \
        return &ptr;                                                            \
    }                                                                           \
                                                                                \
    bool is_class_ptr(void* p_ptr) const override                               \
    {                                                                           \
        return (p_ptr == get_class_ptr_static()) ||                             \
               m_parent::is_class_ptr(p_ptr);                                   \
    }                                                                           \
                                                                                \
    static const StringName& get_class_static()                                 \
    {                                                                           \
        static StringName name(#m_class);                                       \
        return name;                                                            \
    }                                                                           \
                                                                                \
    const StringName& get_class_name() const override                           \
    {                                                                           \
        return get_class_static();                                              \
    }                                                                           \
                                                                                \
    bool is_class(const StringName& p_class) const override                     \
    {                                                                           \
        return (p_class == get_class_static()) ||                               \
               m_parent::is_class(p_class);                                     \
    }                                                                           \
                                                                                \
    static void initialize_class()                                              \
    {                                                                           \
        static bool _initialized = false;                                       \
        if (_initialized)                                                       \
            return;                                                             \
        _initialized = true;                                                    \
        m_parent::initialize_class();                                           \
        Object::_add_class_to_classdb(                                          \
            m_class::get_class_static(),                                        \
            m_parent::get_class_static(),                                       \
            m_class::get_class_ptr_static());                                   \
        m_class::_bind_methods();                                               \
    }                                                                           \
                                                                                \
protected:                                                                      \
    static void (m_class::*_get_notification())(int)                            \
    {                                                                           \
        return &m_class::_notification;                                         \
    }                                                                           \
                                                                                \
    void _notification_dispatch_forward(int p_what) override                    \
    {                                                                           \
        m_parent::_notification_dispatch_forward(p_what);                       \
        if (m_class::_get_notification() != m_parent::_get_notification())      \
        {                                                                       \
            _notification(p_what);                                              \
        }                                                                       \
    }                                                                           \
                                                                                \
    void _notification_dispatch_backward(int p_what) override                   \
    {                                                                           \
        if (m_class::_get_notification() != m_parent::_get_notification())      \
        {                                                                       \
            _notification(p_what);                                              \
        }                                                                       \
        m_parent::_notification_dispatch_backward(p_what);                      \
    }                                                                           \
                                                                                \
    void _initialize_classv() override { initialize_class(); }                  \
                                                                                \
private:                                                                        \
    friend class ClassDB;
// clang-format on
