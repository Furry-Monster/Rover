#pragma once

#include "core/event/event.h"
#include "core/typedefs.h"

#include <memory>

namespace rover
{

    class Window;
    class EventPump;
    class TimeSource;

    // ---------------------------------------------------------------------------
    // LinuxPlatform -- process-wide façade owning the window, event pump,
    // and time source. Singleton-style accessor; init/shutdown are idempotent
    // and called from register_types.cpp.
    // ---------------------------------------------------------------------------
    class LinuxPlatform
    {
    public:
        [[nodiscard]] static LinuxPlatform& get();

        LinuxPlatform(const LinuxPlatform&)            = delete;
        LinuxPlatform& operator=(const LinuxPlatform&) = delete;

        bool init();
        void shutdown();

        [[nodiscard]] Window& window() { return *window_; }

        [[nodiscard]] EventPump& event_pump() { return *event_pump_; }

        [[nodiscard]] TimeSource& time() { return *time_source_; }

        [[nodiscard]] EventBus& events() { return event_bus_; }

        [[nodiscard]] bool initialized() const { return initialized_; }

    private:
        LinuxPlatform() = default;
        ~LinuxPlatform();

        EventBus                    event_bus_;
        std::unique_ptr<Window>     window_;
        std::unique_ptr<EventPump>  event_pump_;
        std::unique_ptr<TimeSource> time_source_;
        bool                        initialized_ = false;
    };

} // namespace rover
