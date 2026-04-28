#include "platform/linux/window.h"

#include "core/log/log.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

namespace rover {

Window::Window() = default;

Window::~Window() {
    shutdown();
}

bool Window::init(const WindowDesc& desc) {
    if (window_ != nullptr) {
        ROVER_LOG_WARN("Window::init called twice; ignoring second call");
        return true;
    }

    SDL_WindowFlags flags = 0;
    if (desc.vulkan) {
        flags |= SDL_WINDOW_VULKAN;
    }
    if (desc.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    window_ = SDL_CreateWindow(
        desc.title,
        static_cast<int>(desc.width),
        static_cast<int>(desc.height),
        flags);

    if (window_ == nullptr) {
        ROVER_LOG_ERROR("SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }

    width_        = desc.width;
    height_       = desc.height;
    should_close_ = false;

    ROVER_LOG_INFO("Window created: '{}' {}x{} (vulkan={}, resizable={})",
                   desc.title, desc.width, desc.height,
                   desc.vulkan, desc.resizable);
    return true;
}

void Window::shutdown() {
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    width_        = 0;
    height_       = 0;
    should_close_ = false;
}

bool Window::create_vulkan_surface(void* instance, void** surface_out) {
    if (window_ == nullptr) {
        ROVER_LOG_ERROR("create_vulkan_surface called before window init");
        return false;
    }
    if (surface_out == nullptr) {
        ROVER_LOG_ERROR("create_vulkan_surface: surface_out is null");
        return false;
    }

    VkInstance    vk_inst     = static_cast<VkInstance>(instance);
    VkSurfaceKHR* surface_ptr = reinterpret_cast<VkSurfaceKHR*>(surface_out);

    if (!SDL_Vulkan_CreateSurface(window_, vk_inst, nullptr, surface_ptr)) {
        ROVER_LOG_ERROR("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        return false;
    }
    return true;
}

void Window::get_vulkan_required_extensions(std::vector<const char*>& out) {
    Uint32 count = 0;
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&count);
    if (exts == nullptr) {
        ROVER_LOG_ERROR("SDL_Vulkan_GetInstanceExtensions failed: {}",
                        SDL_GetError());
        return;
    }
    out.reserve(out.size() + count);
    for (Uint32 i = 0; i < count; ++i) {
        out.push_back(exts[i]);
    }
}

} // namespace rover
