#pragma once

#include "core/object/property_info.h"
#include "core/object/object_macros.h"

#include <atomic>

namespace rover {

class Object {
public:
    Object();
    virtual ~Object() = default;

    virtual StringName get_class_name() const;
    static StringName get_class_name_static();
    static StringName get_parent_class_name_static();

    virtual bool is_class(const StringName& name) const;

    u64 get_instance_id() const { return instance_id_; }

private:
    u64 instance_id_;
    static std::atomic<u64> s_next_instance_id;
};

} // namespace rover
