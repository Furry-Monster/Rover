#include "platform/linux/linux_platform.h"

#include "core/log/log.h"
#include "platform/linux/event_pump.h"
#include "platform/linux/time_source.h"
#include "platform/linux/window.h"

#include <SDL3/SDL.h>

namespace rover {

LinuxPlatform& LinuxPlatform::get() {
    static LinuxPlatform instance;
    return instance;
}

LinuxPlatform::~LinuxPlatform() {
    if (initialized_) {
        shutdown();
    }
}

bool LinuxPlatform::init() {
    if (initialized_) {
        ROVER_LOG_WARN("LinuxPlatform::init called twice; ignoring");
        return true;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        ROVER_LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        return false;
    }

    window_ = std::make_unique<Window>();
    if (!window_->init(WindowDesc{})) {
        ROVER_LOG_ERROR("LinuxPlatform: window initialization failed");
        window_.reset();
        SDL_Quit();
        return false;
    }

    event_pump_  = std::make_unique<EventPump>(event_bus_, *window_);
    time_source_ = std::make_unique<TimeSource>();

    initialized_ = true;
    ROVER_LOG_INFO("LinuxPlatform initialized (SDL {}.{}.{})",
                   SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
    return true;
}

void LinuxPlatform::shutdown() {
    if (!initialized_) {
        return;
    }

    time_source_.reset();
    event_pump_.reset();
    if (window_) {
        window_->shutdown();
        window_.reset();
    }

    SDL_Quit();
    initialized_ = false;
}

} // namespace rover
