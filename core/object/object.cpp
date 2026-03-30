#include "core/object/object.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

uint64_t Object::_next_instance_id = 0;

// -- ClassDB bridge ---------------------------------------------------------

void
Object::_add_class_to_classdb(const StringName& p_name, const StringName& p_parent, void* p_ptr)
{
    ClassDB::_add_class(p_name, p_parent, p_ptr);
}

void
Object::initialize_class()
{
    static bool initialized = false;
    if (initialized)
    {
        return;
    }
    initialized = true;

    ClassDB::_add_class(get_class_static(), StringName(), get_class_ptr_static());
}

// -- Lifecycle --------------------------------------------------------------

Object::Object() : _instance_id(++_next_instance_id) {}

Object::~Object() = default;

// -- Notifications ----------------------------------------------------------

void
Object::notification(int p_what, bool p_reversed)
{
    if (p_reversed)
    {
        _notification_dispatch_backward(p_what);
    }
    else
    {
        _notification_dispatch_forward(p_what);
    }
}

void
Object::_notification_dispatch_forward(int p_what)
{
    _notification(p_what);
}

void
Object::_notification_dispatch_backward(int p_what)
{
    _notification(p_what);
}

// -- Signals ----------------------------------------------------------------

Error
Object::connect(const StringName& p_signal, const Callable& p_callable)
{
    if (!p_callable.is_valid())
    {
        return FAILED;
    }
    _signal_map[p_signal].push_back({p_callable});
    return OK;
}

Error
Object::connect(const StringName& p_signal, Object* p_target, const StringName& p_method)
{
    return connect(p_signal, Callable(p_target, p_method));
}

void
Object::disconnect(const StringName& p_signal, Object* p_target, const StringName& p_method)
{
    auto it = _signal_map.find(p_signal);
    if (it == _signal_map.end())
    {
        return;
    }
    Callable match(p_target, p_method);
    auto&    conns = it->second;
    conns.erase(
        std::remove_if(conns.begin(), conns.end(), [&match](const SignalConnection& c) { return c.callable == match; }),
        conns.end());
}

bool
Object::is_connected(const StringName& p_signal, Object* p_target, const StringName& p_method) const
{
    auto it = _signal_map.find(p_signal);
    if (it == _signal_map.end())
    {
        return false;
    }
    Callable match(p_target, p_method);
    for (const auto& conn : it->second)
    {
        if (conn.callable == match)
        {
            return true;
        }
    }
    return false;
}

void
Object::emit_signal_argv(const StringName& p_signal, const Variant* p_args, int p_arg_count)
{
    auto it = _signal_map.find(p_signal);
    if (it == _signal_map.end())
    {
        return;
    }
    // Copy connection list in case a handler modifies connections.
    auto conns = it->second;
    for (const auto& conn : conns)
    {
        conn.callable.call(p_args, p_arg_count);
    }
}

// -- Virtual method call ----------------------------------------------------

Variant
Object::call(const StringName& /*p_method*/, const Variant* /*p_args*/, int /*p_arg_count*/)
{
    return {};
}
