#pragma once

#include "core/typedefs.h"
#include "drivers/vulkan/vk_common.h"

namespace rover
{

    struct QueueFamilyIndices
    {
        u32 graphics = ~0u;
        u32 present  = ~0u;

        [[nodiscard]] bool is_complete() const { return graphics != ~0u && present != ~0u; }
    };

    class VkDeviceWrapper
    {
    public:
        bool init(VkInstance instance, VkSurfaceKHR surface);
        void shutdown();

        [[nodiscard]] VkPhysicalDevice physical() const { return physical_; }

        [[nodiscard]] VkDevice logical() const { return device_; }

        [[nodiscard]] VkQueue graphics_queue() const { return graphics_queue_; }

        [[nodiscard]] VkQueue present_queue() const { return present_queue_; }

        [[nodiscard]] const QueueFamilyIndices& queue_families() const { return queue_families_; }

        [[nodiscard]] const char* device_name() const { return device_name_; }

        [[nodiscard]] const VkPhysicalDeviceProperties& properties() const { return properties_; }

    private:
        VkPhysicalDevice           physical_       = VK_NULL_HANDLE;
        VkDevice                   device_         = VK_NULL_HANDLE;
        VkQueue                    graphics_queue_ = VK_NULL_HANDLE;
        VkQueue                    present_queue_  = VK_NULL_HANDLE;
        QueueFamilyIndices         queue_families_;
        VkPhysicalDeviceProperties properties_{};
        char                       device_name_[256] = {};
    };

} // namespace rover
