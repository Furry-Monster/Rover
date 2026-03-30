#include "core/object/object.h"

#include "core/object/class_db.h"

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
