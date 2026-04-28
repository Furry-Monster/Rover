#pragma once

#include "core/typedefs.h"

namespace rover {

// ---------------------------------------------------------------------------
// TimeSource -- high-precision wall-clock and frame timing using SDL3's
// performance counter. Construct after SDL has been initialized; call
// `tick()` exactly once per frame to advance delta.
// ---------------------------------------------------------------------------
class TimeSource {
public:
    TimeSource();

    // Advances current time and computes delta from the previous tick.
    void tick();

    [[nodiscard]] f64 elapsed_seconds() const;
    [[nodiscard]] f64 delta_seconds()   const;
    [[nodiscard]] u64 elapsed_micros()  const;
    [[nodiscard]] u64 frame_count()     const;

private:
    u64 frequency_       = 0;     // SDL_GetPerformanceFrequency() at construction
    u64 start_counter_   = 0;     // counter at construction
    u64 last_counter_    = 0;     // counter at last tick (or start if never ticked)
    u64 current_counter_ = 0;     // counter at most recent tick
    f64 delta_seconds_   = 0.0;
    u64 frame_count_     = 0;
};

} // namespace rover
