#pragma once

#include "core/graphics/window_system.h"
#include "core/typedefs.h"

#include <vector>

struct SDL_Window;

namespace rover {

// ---------------------------------------------------------------------------
// WindowDesc -- creation parameters for Window::init().
// ---------------------------------------------------------------------------
struct WindowDesc {
    const char* title     = "Rover";
    u32         width     = 1280;
    u32         height    = 720;
    bool        resizable = true;
    bool        vulkan    = true;
};

// ---------------------------------------------------------------------------
// Window -- SDL3 implementation of the WindowSystem interface. Owns a
// single SDL_Window, optionally Vulkan-capable.
// ---------------------------------------------------------------------------
class Window : public WindowSystem {
public:
    Window();
    ~Window() override;

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    bool init(const WindowDesc& desc);
    void shutdown();

    // ---- WindowSystem ----
    bool create_vulkan_surface(void* instance, void** surface_out) override;
    void get_vulkan_required_extensions(std::vector<const char*>& out) override;

    [[nodiscard]] u32  get_width()    const override { return width_; }
    [[nodiscard]] u32  get_height()   const override { return height_; }
    [[nodiscard]] bool should_close() const override { return should_close_; }

    // ---- Native handle access (consumed by EventPump) ----
    [[nodiscard]] SDL_Window* native() const { return window_; }

    void mark_should_close()           { should_close_ = true; }
    void set_size(u32 w, u32 h)        { width_ = w; height_ = h; }

private:
    SDL_Window* window_       = nullptr;
    u32         width_        = 0;
    u32         height_       = 0;
    bool        should_close_ = false;
};

} // namespace rover
