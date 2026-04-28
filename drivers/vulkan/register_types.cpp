#include "drivers/vulkan/register_types.h"

#include "core/log/log.h"
#include "drivers/vulkan/graphics_device_vulkan.h"

namespace rover {

namespace {
GraphicsDeviceVulkan* g_vulkan_device = nullptr;
} // namespace

GraphicsDevice* get_vulkan_device() {
    return g_vulkan_device;
}

void register_vulkan_driver() {
    if (g_vulkan_device != nullptr) {
        ROVER_LOG_WARN("Vulkan driver already registered");
        return;
    }
    g_vulkan_device = new GraphicsDeviceVulkan();
    ROVER_LOG_INFO("Vulkan driver instance created (init deferred)");
}

void unregister_vulkan_driver() {
    if (g_vulkan_device != nullptr) {
        delete g_vulkan_device;
        g_vulkan_device = nullptr;
    }
    ROVER_LOG_INFO("Vulkan driver instance destroyed");
}

} // namespace rover
