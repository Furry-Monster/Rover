#include "core/register_core_types.h"

#include "core/log/log.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/variant/variant.h"

namespace rover
{

    void register_core_types()
    {
        Log::init();

        ClassDB::register_class<Object>();
        ClassDB::register_class<RefCounted>();

        // Variant is value type (not Object), no ClassDB entry. Touch to assert
        // the static_assert in variant.h fires at compile time only.
        static_assert(sizeof(Variant) <= 80, "Variant must remain compact");

        ROVER_LOG_INFO("Core types registered");
    }

    void unregister_core_types()
    {
        ROVER_LOG_INFO("Core types unregistered");
        Log::shutdown();
    }

} // namespace rover
