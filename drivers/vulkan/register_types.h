#pragma once

namespace rover
{

    class GraphicsDevice;

    // Returns the registered Vulkan GraphicsDevice (abstract base).
    // Callers in main/services should use this pointer; including
    // graphics_device_vulkan.h is unnecessary and would leak Vulkan +
    // X11 headers (X11 #defines `None` which collides with engine enums).
    [[nodiscard]] GraphicsDevice* get_vulkan_device();

    void register_vulkan_driver();
    void unregister_vulkan_driver();

} // namespace rover
