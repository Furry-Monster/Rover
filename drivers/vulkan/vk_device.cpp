#include "drivers/vulkan/vk_device.h"

#include "core/log/log.h"

#include <array>
#include <cstring>
#include <set>
#include <vector>

namespace rover
{

    namespace
    {

        constexpr std::array<const char*, 1> kRequiredDeviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        bool device_supports_extensions(VkPhysicalDevice physical)
        {
            u32 count = 0;
            if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr) != VK_SUCCESS)
            {
                return false;
            }
            std::vector<VkExtensionProperties> available(count);
            if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, available.data()) != VK_SUCCESS)
            {
                return false;
            }
            for (const char* req : kRequiredDeviceExtensions)
            {
                bool found = false;
                for (const auto& ext : available)
                {
                    if (std::strcmp(ext.extensionName, req) == 0)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    return false;
                }
            }
            return true;
        }

        QueueFamilyIndices find_queue_families(VkPhysicalDevice physical, VkSurfaceKHR surface)
        {
            QueueFamilyIndices indices;

            u32 count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());

            for (u32 i = 0; i < count; ++i)
            {
                if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    if (indices.graphics == ~0u)
                    {
                        indices.graphics = i;
                    }
                }

                VkBool32 present_support = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(physical, i, surface, &present_support);
                if (present_support == VK_TRUE)
                {
                    if (indices.present == ~0u)
                    {
                        indices.present = i;
                    }
                }

                if (indices.is_complete())
                {
                    break;
                }
            }

            return indices;
        }

        bool surface_has_formats(VkPhysicalDevice physical, VkSurfaceKHR surface)
        {
            u32 fmt_count = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &fmt_count, nullptr);
            u32 mode_count = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &mode_count, nullptr);
            return fmt_count > 0 && mode_count > 0;
        }

        i32 score_device(VkPhysicalDevice physical, VkSurfaceKHR surface)
        {
            if (!device_supports_extensions(physical))
            {
                return -1;
            }
            if (!surface_has_formats(physical, surface))
            {
                return -1;
            }
            if (!find_queue_families(physical, surface).is_complete())
            {
                return -1;
            }

            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(physical, &props);

            i32 score = 100;
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                score += 1000;
            }
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            {
                score += 500;
            }
            return score;
        }

    } // namespace

    bool VkDeviceWrapper::init(VkInstance instance, VkSurfaceKHR surface)
    {
        u32 device_count = 0;
        VK_CHECK_RETURN(vkEnumeratePhysicalDevices(instance, &device_count, nullptr), false);
        if (device_count == 0)
        {
            ROVER_LOG_ERROR("No Vulkan-capable physical devices found");
            return false;
        }
        std::vector<VkPhysicalDevice> devices(device_count);
        VK_CHECK_RETURN(vkEnumeratePhysicalDevices(instance, &device_count, devices.data()), false);

        i32              best_score  = -1;
        VkPhysicalDevice best_device = VK_NULL_HANDLE;
        for (VkPhysicalDevice candidate : devices)
        {
            const i32 score = score_device(candidate, surface);
            if (score > best_score)
            {
                best_score  = score;
                best_device = candidate;
            }
        }

        if (best_device == VK_NULL_HANDLE)
        {
            ROVER_LOG_ERROR("No suitable Vulkan physical device");
            return false;
        }

        physical_ = best_device;
        vkGetPhysicalDeviceProperties(physical_, &properties_);
        std::strncpy(device_name_, properties_.deviceName, sizeof(device_name_) - 1);
        device_name_[sizeof(device_name_) - 1] = '\0';
        ROVER_LOG_INFO("Vulkan physical device: {}", device_name_);

        queue_families_ = find_queue_families(physical_, surface);
        if (!queue_families_.is_complete())
        {
            ROVER_LOG_ERROR("Selected device missing required queue families");
            return false;
        }

        std::set<u32>                        unique_families = {queue_families_.graphics, queue_families_.present};
        std::vector<VkDeviceQueueCreateInfo> queue_infos;
        queue_infos.reserve(unique_families.size());
        const f32 priority = 1.0f;
        for (u32 family : unique_families)
        {
            VkDeviceQueueCreateInfo info{};
            info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            info.queueFamilyIndex = family;
            info.queueCount       = 1;
            info.pQueuePriorities = &priority;
            queue_infos.push_back(info);
        }

        VkPhysicalDeviceFeatures features{};
        features.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo device_info{};
        device_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount    = static_cast<u32>(queue_infos.size());
        device_info.pQueueCreateInfos       = queue_infos.data();
        device_info.enabledExtensionCount   = static_cast<u32>(kRequiredDeviceExtensions.size());
        device_info.ppEnabledExtensionNames = kRequiredDeviceExtensions.data();
        device_info.pEnabledFeatures        = &features;

        VK_CHECK_RETURN(vkCreateDevice(physical_, &device_info, nullptr, &device_), false);
        volkLoadDevice(device_);

        vkGetDeviceQueue(device_, queue_families_.graphics, 0, &graphics_queue_);
        vkGetDeviceQueue(device_, queue_families_.present, 0, &present_queue_);

        ROVER_LOG_INFO("Vulkan logical device created (graphics_q={}, present_q={})",
                       queue_families_.graphics,
                       queue_families_.present);
        return true;
    }

    void VkDeviceWrapper::shutdown()
    {
        if (device_ != VK_NULL_HANDLE)
        {
            vkDestroyDevice(device_, nullptr);
            device_ = VK_NULL_HANDLE;
        }
        physical_       = VK_NULL_HANDLE;
        graphics_queue_ = VK_NULL_HANDLE;
        present_queue_  = VK_NULL_HANDLE;
        queue_families_ = {};
        properties_     = {};
        device_name_[0] = '\0';
    }

} // namespace rover
