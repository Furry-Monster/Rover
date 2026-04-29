#include "core/object/object.h"

namespace rover
{

    std::atomic<u64> Object::s_next_instance_id{1};

    Object::Object() : instance_id_(s_next_instance_id.fetch_add(1, std::memory_order_relaxed)) {}

    StringName Object::get_class_name() const
    {
        return "Object";
    }

    StringName Object::get_class_name_static()
    {
        return "Object";
    }

    StringName Object::get_parent_class_name_static()
    {
        return "";
    }

    bool Object::is_class(const StringName& name) const
    {
        return name == "Object";
    }

} // namespace rover
