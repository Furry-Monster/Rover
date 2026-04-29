#include "core/object/ref_counted.h"

namespace rover
{

    void RefCounted::add_ref()
    {
        ref_count_.fetch_add(1, std::memory_order_relaxed);
    }

    bool RefCounted::release()
    {
        u32 prev = ref_count_.fetch_sub(1, std::memory_order_acq_rel);
        return prev == 1;
    }

    u32 RefCounted::ref_count() const
    {
        return ref_count_.load(std::memory_order_relaxed);
    }

} // namespace rover
