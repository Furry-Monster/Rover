#pragma once

#include "core/typedefs.h"
#include "drivers/vulkan/vk_common.h"
#include "drivers/vulkan/vk_device.h"

#include <vector>

namespace rover
{

    class VkSwapchainWrapper
    {
    public:
        bool init(VkInstance                instance,
                  VkPhysicalDevice          physical,
                  VkDevice                  device,
                  VkSurfaceKHR              surface,
                  const QueueFamilyIndices& families,
                  u32                       width,
                  u32                       height);
        void shutdown(VkDevice device);
        bool recreate(VkPhysicalDevice          physical,
                      VkDevice                  device,
                      VkSurfaceKHR              surface,
                      const QueueFamilyIndices& families,
                      u32                       width,
                      u32                       height);

        [[nodiscard]] VkSwapchainKHR handle() const { return swapchain_; }

        [[nodiscard]] VkFormat image_format() const { return image_format_; }

        [[nodiscard]] VkExtent2D extent() const { return extent_; }

        [[nodiscard]] u32 image_count() const { return static_cast<u32>(images_.size()); }

        [[nodiscard]] const std::vector<VkImage>& images() const { return images_; }

        [[nodiscard]] const std::vector<VkImageView>& image_views() const { return image_views_; }

        // Per-image render-finished semaphores. There must be one per swapchain
        // image (NOT per frame-in-flight) because the wait by vkQueuePresentKHR
        // is tied to the image, not to the frame slot.
        [[nodiscard]] VkSemaphore render_finished_semaphore(u32 image_index) const;

        bool acquire_next_image(VkDevice device, VkSemaphore signal_sem, u32* image_index_out);
        bool present(VkQueue queue, VkSemaphore wait_sem, u32 image_index);

    private:
        bool create_internal(VkPhysicalDevice          physical,
                             VkDevice                  device,
                             VkSurfaceKHR              surface,
                             const QueueFamilyIndices& families,
                             u32                       width,
                             u32                       height);
        void destroy_internal(VkDevice device);

        VkSwapchainKHR           swapchain_    = VK_NULL_HANDLE;
        VkFormat                 image_format_ = VK_FORMAT_UNDEFINED;
        VkExtent2D               extent_       = {};
        std::vector<VkImage>     images_;
        std::vector<VkImageView> image_views_;
        std::vector<VkSemaphore> render_finished_semaphores_;
    };

} // namespace rover
