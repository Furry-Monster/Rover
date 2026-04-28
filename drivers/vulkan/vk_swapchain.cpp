#include "drivers/vulkan/vk_swapchain.h"

#include "core/log/log.h"

#include <algorithm>
#include <array>

namespace rover {

namespace {

VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats[0];
}

VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& modes) {
    for (VkPresentModeKHR m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& caps, u32 width, u32 height) {
    if (caps.currentExtent.width != UINT32_MAX) {
        return caps.currentExtent;
    }
    VkExtent2D out{width, height};
    out.width  = std::clamp(out.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    out.height = std::clamp(out.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return out;
}

} // namespace

bool VkSwapchainWrapper::init(VkInstance /*instance*/, VkPhysicalDevice physical, VkDevice device,
                              VkSurfaceKHR surface, const QueueFamilyIndices& families,
                              u32 width, u32 height) {
    return create_internal(physical, device, surface, families, width, height);
}

void VkSwapchainWrapper::shutdown(VkDevice device) {
    destroy_internal(device);
}

bool VkSwapchainWrapper::recreate(VkPhysicalDevice physical, VkDevice device, VkSurfaceKHR surface,
                                  const QueueFamilyIndices& families, u32 width, u32 height) {
    destroy_internal(device);
    return create_internal(physical, device, surface, families, width, height);
}

bool VkSwapchainWrapper::create_internal(VkPhysicalDevice physical, VkDevice device,
                                         VkSurfaceKHR surface, const QueueFamilyIndices& families,
                                         u32 width, u32 height) {
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK_RETURN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps), false);

    u32 fmt_count = 0;
    VK_CHECK_RETURN(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &fmt_count, nullptr), false);
    std::vector<VkSurfaceFormatKHR> formats(fmt_count);
    VK_CHECK_RETURN(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &fmt_count, formats.data()), false);

    u32 mode_count = 0;
    VK_CHECK_RETURN(vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &mode_count, nullptr), false);
    std::vector<VkPresentModeKHR> modes(mode_count);
    VK_CHECK_RETURN(vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &mode_count, modes.data()), false);

    if (formats.empty() || modes.empty()) {
        ROVER_LOG_ERROR("Surface has no available formats or present modes");
        return false;
    }

    const VkSurfaceFormatKHR surface_format = choose_surface_format(formats);
    const VkPresentModeKHR   present_mode   = choose_present_mode(modes);
    const VkExtent2D         chosen_extent  = choose_extent(caps, width, height);

    if (chosen_extent.width == 0 || chosen_extent.height == 0) {
        ROVER_LOG_WARN("Swapchain extent has zero dimension; deferring creation");
        return false;
    }

    u32 image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR info{};
    info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface          = surface;
    info.minImageCount    = image_count;
    info.imageFormat      = surface_format.format;
    info.imageColorSpace  = surface_format.colorSpace;
    info.imageExtent      = chosen_extent;
    info.imageArrayLayers = 1;
    info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    std::array<u32, 2> family_indices = {families.graphics, families.present};
    if (families.graphics != families.present) {
        info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices   = family_indices.data();
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform   = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode    = present_mode;
    info.clipped        = VK_TRUE;
    info.oldSwapchain   = VK_NULL_HANDLE;

    VK_CHECK_RETURN(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain_), false);

    image_format_ = surface_format.format;
    extent_       = chosen_extent;

    u32 actual_count = 0;
    VK_CHECK_RETURN(vkGetSwapchainImagesKHR(device, swapchain_, &actual_count, nullptr), false);
    images_.resize(actual_count);
    VK_CHECK_RETURN(vkGetSwapchainImagesKHR(device, swapchain_, &actual_count, images_.data()), false);

    image_views_.resize(actual_count, VK_NULL_HANDLE);
    for (u32 i = 0; i < actual_count; ++i) {
        VkImageViewCreateInfo view_info{};
        view_info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image    = images_[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format   = image_format_;
        view_info.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        };
        view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel   = 0;
        view_info.subresourceRange.levelCount     = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount     = 1;
        VK_CHECK_RETURN(vkCreateImageView(device, &view_info, nullptr, &image_views_[i]), false);
    }

    render_finished_semaphores_.resize(actual_count, VK_NULL_HANDLE);
    VkSemaphoreCreateInfo sem_info{};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (u32 i = 0; i < actual_count; ++i) {
        VK_CHECK_RETURN(vkCreateSemaphore(device, &sem_info, nullptr,
                                          &render_finished_semaphores_[i]), false);
    }

    ROVER_LOG_INFO("Vulkan swapchain created ({}x{}, {} images)",
                   extent_.width, extent_.height, actual_count);
    return true;
}

void VkSwapchainWrapper::destroy_internal(VkDevice device) {
    for (VkSemaphore sem : render_finished_semaphores_) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, sem, nullptr);
        }
    }
    render_finished_semaphores_.clear();

    for (VkImageView view : image_views_) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
        }
    }
    image_views_.clear();
    images_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    image_format_ = VK_FORMAT_UNDEFINED;
    extent_       = {};
}

VkSemaphore VkSwapchainWrapper::render_finished_semaphore(u32 image_index) const {
    if (image_index >= render_finished_semaphores_.size()) {
        return VK_NULL_HANDLE;
    }
    return render_finished_semaphores_[image_index];
}

bool VkSwapchainWrapper::acquire_next_image(VkDevice device, VkSemaphore signal_sem,
                                            u32* image_index_out) {
    const VkResult result = vkAcquireNextImageKHR(
        device, swapchain_, UINT64_MAX, signal_sem, VK_NULL_HANDLE, image_index_out);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        ROVER_LOG_ERROR("vkAcquireNextImageKHR failed: {}", static_cast<i32>(result));
        return false;
    }
    return true;
}

bool VkSwapchainWrapper::present(VkQueue queue, VkSemaphore wait_sem, u32 image_index) {
    VkPresentInfoKHR info{};
    info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = wait_sem != VK_NULL_HANDLE ? 1u : 0u;
    info.pWaitSemaphores    = wait_sem != VK_NULL_HANDLE ? &wait_sem : nullptr;
    info.swapchainCount     = 1;
    info.pSwapchains        = &swapchain_;
    info.pImageIndices      = &image_index;

    const VkResult result = vkQueuePresentKHR(queue, &info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return false;
    }
    if (result != VK_SUCCESS) {
        ROVER_LOG_ERROR("vkQueuePresentKHR failed: {}", static_cast<i32>(result));
        return false;
    }
    return true;
}

} // namespace rover
