#pragma once

#include "core/typedefs.h"

#include <vector>

namespace rover {

// ---------------------------------------------------------------------------
// WindowSystem -- abstract interface bridging the platform layer and the
// graphics driver. Implemented by platform/<os>/, consumed by drivers/<api>/.
//
// Vulkan handles are passed as opaque void* to avoid leaking vendor headers
// into core. Implementations must reinterpret_cast to the proper VkInstance
// / VkSurfaceKHR types.
// ---------------------------------------------------------------------------
class WindowSystem {
public:
    virtual ~WindowSystem() = default;

    // ---- Vulkan integration ----

    // Create a VkSurfaceKHR for this window.
    //   instance:    VkInstance (passed as void*)
    //   surface_out: VkSurfaceKHR* (output, written via void**)
    // Returns true on success.
    virtual bool create_vulkan_surface(void* instance, void** surface_out) = 0;

    // Append the platform-required Vulkan instance extension names to `out`.
    // Strings have static lifetime owned by the platform.
    virtual void get_vulkan_required_extensions(std::vector<const char*>& out) = 0;

    // ---- Window properties ----

    [[nodiscard]] virtual u32 get_width() const = 0;
    [[nodiscard]] virtual u32 get_height() const = 0;

    // True after the user requested window close (e.g. clicked the X).
    [[nodiscard]] virtual bool should_close() const = 0;
};

} // namespace rover
