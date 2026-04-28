#pragma once

#include "core/object/object.h"

#include <atomic>

namespace rover {

class RefCounted : public Object {
    ROVER_CLASS(RefCounted, Object)

public:
    RefCounted() = default;

    void add_ref();
    bool release();
    u32 ref_count() const;

private:
    std::atomic<u32> ref_count_{1};
};

} // namespace rover
