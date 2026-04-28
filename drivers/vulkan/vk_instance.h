#pragma once

#include "core/typedefs.h"
#include "drivers/vulkan/vk_common.h"

#include <vector>

namespace rover {

class VkInstanceWrapper {
public:
    bool init(const std::vector<const char*>& required_extensions, bool enable_validation);
    void shutdown();

    [[nodiscard]] VkInstance handle() const { return instance_; }
    [[nodiscard]] bool       validation_enabled() const { return debug_messenger_ != VK_NULL_HANDLE; }

private:
    VkInstance               instance_        = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
};

} // namespace rover
