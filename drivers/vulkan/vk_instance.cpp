#include "drivers/vulkan/vk_instance.h"

#include "core/log/log.h"

#include <cstring>

namespace rover
{

    namespace
    {

        constexpr const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";

        VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                                VkDebugUtilsMessageTypeFlagsEXT /*type*/,
                                                                const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                                void* /*user_data*/)
        {
            const char* msg = (data && data->pMessage) ? data->pMessage : "(null)";
            if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            {
                ROVER_LOG_ERROR("[Vulkan] {}", msg);
            }
            else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            {
                ROVER_LOG_WARN("[Vulkan] {}", msg);
            }
            else
            {
                ROVER_LOG_TRACE("[Vulkan] {}", msg);
            }
            return VK_FALSE;
        }

        bool validation_layer_available()
        {
            u32 count = 0;
            if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS || count == 0)
            {
                return false;
            }
            std::vector<VkLayerProperties> layers(count);
            if (vkEnumerateInstanceLayerProperties(&count, layers.data()) != VK_SUCCESS)
            {
                return false;
            }
            for (const auto& layer : layers)
            {
                if (std::strcmp(layer.layerName, kValidationLayerName) == 0)
                {
                    return true;
                }
            }
            return false;
        }

        void populate_debug_messenger_info(VkDebugUtilsMessengerCreateInfoEXT& info)
        {
            info       = {};
            info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            info.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            info.pfnUserCallback = debug_messenger_callback;
        }

    } // namespace

    bool VkInstanceWrapper::init(const std::vector<const char*>& required_extensions, bool enable_validation)
    {
        VK_CHECK_RETURN(volkInitialize(), false);

        bool use_validation = enable_validation && validation_layer_available();
        if (enable_validation && !use_validation)
        {
            ROVER_LOG_WARN("Vulkan validation layer not available; continuing without");
        }

        std::vector<const char*> extensions = required_extensions;
        if (use_validation)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        std::vector<const char*> layers;
        if (use_validation)
        {
            layers.push_back(kValidationLayerName);
        }

        VkApplicationInfo app_info{};
        app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName   = "Rover";
        app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        app_info.pEngineName        = "Rover";
        app_info.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
        app_info.apiVersion         = VK_API_VERSION_1_3;

        VkDebugUtilsMessengerCreateInfoEXT debug_info{};
        populate_debug_messenger_info(debug_info);

        VkInstanceCreateInfo create_info{};
        create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo        = &app_info;
        create_info.enabledExtensionCount   = static_cast<u32>(extensions.size());
        create_info.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
        create_info.enabledLayerCount       = static_cast<u32>(layers.size());
        create_info.ppEnabledLayerNames     = layers.empty() ? nullptr : layers.data();
        if (use_validation)
        {
            create_info.pNext = &debug_info;
        }

        VK_CHECK_RETURN(vkCreateInstance(&create_info, nullptr, &instance_), false);
        volkLoadInstance(instance_);

        if (use_validation)
        {
            if (vkCreateDebugUtilsMessengerEXT)
            {
                VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance_, &debug_info, nullptr, &debug_messenger_));
            }
            else
            {
                ROVER_LOG_WARN("vkCreateDebugUtilsMessengerEXT not loaded; skipping debug messenger");
            }
        }

        ROVER_LOG_INFO("Vulkan instance created (validation={})", use_validation);
        return true;
    }

    void VkInstanceWrapper::shutdown()
    {
        if (debug_messenger_ != VK_NULL_HANDLE)
        {
            if (vkDestroyDebugUtilsMessengerEXT)
            {
                vkDestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
            }
            debug_messenger_ = VK_NULL_HANDLE;
        }
        if (instance_ != VK_NULL_HANDLE)
        {
            vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
        }
    }

} // namespace rover
