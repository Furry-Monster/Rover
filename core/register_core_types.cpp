#include "core/register_core_types.h"

#include "core/log/log.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"

namespace rover {

void register_core_types() {
    Log::init();

    ClassDB::register_class<Object>();
    ClassDB::register_class<RefCounted>();

    ROVER_LOG_INFO("Core types registered");
}

void unregister_core_types() {
    ROVER_LOG_INFO("Core types unregistered");
    Log::shutdown();
}

} // namespace rover
