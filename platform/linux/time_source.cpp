#include "platform/linux/time_source.h"

#include <SDL3/SDL.h>

namespace rover
{

    TimeSource::TimeSource() : frequency_(SDL_GetPerformanceFrequency()), start_counter_(SDL_GetPerformanceCounter())
    {
        last_counter_    = start_counter_;
        current_counter_ = start_counter_;
    }

    void TimeSource::tick()
    {
        last_counter_    = current_counter_;
        current_counter_ = SDL_GetPerformanceCounter();

        const u64 ticks = (current_counter_ > last_counter_) ? (current_counter_ - last_counter_) : 0;
        delta_seconds_  = (frequency_ > 0) ? static_cast<f64>(ticks) / static_cast<f64>(frequency_) : 0.0;
        ++frame_count_;
    }

    f64 TimeSource::elapsed_seconds() const
    {
        if (frequency_ == 0)
        {
            return 0.0;
        }
        const u64 now   = SDL_GetPerformanceCounter();
        const u64 ticks = (now > start_counter_) ? (now - start_counter_) : 0;
        return static_cast<f64>(ticks) / static_cast<f64>(frequency_);
    }

    f64 TimeSource::delta_seconds() const
    {
        return delta_seconds_;
    }

    u64 TimeSource::elapsed_micros() const
    {
        if (frequency_ == 0)
        {
            return 0;
        }
        const u64 now   = SDL_GetPerformanceCounter();
        const u64 ticks = (now > start_counter_) ? (now - start_counter_) : 0;
        // Multiply before divide to preserve precision; ticks * 1e6 fits unless
        // the program runs for ~hundreds of years on typical 10MHz frequencies.
        return (ticks * 1000000ull) / frequency_;
    }

    u64 TimeSource::frame_count() const
    {
        return frame_count_;
    }

} // namespace rover
