#pragma once

#include "core/log/log.h"
#include "core/typedefs.h"

#include <volk.h>

#define VK_CHECK(expr)                                                          \
    do {                                                                        \
        VkResult _result = (expr);                                              \
        if (_result != VK_SUCCESS) {                                            \
            ROVER_LOG_ERROR("Vulkan error {} at {}:{} : " #expr,                \
                            static_cast<::rover::i32>(_result),                 \
                            __FILE__, __LINE__);                                \
        }                                                                       \
    } while (0)

#define VK_CHECK_RETURN(expr, ret_val)                                          \
    do {                                                                        \
        VkResult _result = (expr);                                              \
        if (_result != VK_SUCCESS) {                                            \
            ROVER_LOG_ERROR("Vulkan error {} at {}:{} : " #expr,                \
                            static_cast<::rover::i32>(_result),                 \
                            __FILE__, __LINE__);                                \
            return (ret_val);                                                   \
        }                                                                       \
    } while (0)

namespace rover {

inline constexpr u32 kMaxFramesInFlight = 2;

} // namespace rover
