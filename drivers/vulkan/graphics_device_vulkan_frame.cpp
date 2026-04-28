#include "drivers/vulkan/graphics_device_vulkan.h"

#include "core/log/log.h"
#include "drivers/vulkan/vk_format.h"

#include <vector>

namespace rover {

bool GraphicsDeviceVulkan::begin_frame() {
    if (in_frame_) {
        ROVER_LOG_WARN("begin_frame called while already in a frame");
        return false;
    }

    if (swapchain_dirty_ || swapchain_.handle() == VK_NULL_HANDLE) {
        recreate_swapchain();
        if (swapchain_.handle() == VK_NULL_HANDLE) {
            return false;
        }
    }

    FrameSync& fs = frame_sync_[current_frame_];
    VK_CHECK(vkWaitForFences(device_.logical(), 1, &fs.in_flight, VK_TRUE, UINT64_MAX));

    // Fence signaled => GPU finished with the command buffers we retired the
    // last time this frame slot was used. Free them now so the pool stays bounded.
    auto& retired = retired_command_lists_[current_frame_];
    for (CommandListHandle h : retired) {
        VkCommandListResource res{};
        if (command_lists_.remove(h, &res)) {
            if (res.buffer != VK_NULL_HANDLE && command_pool_ != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(device_.logical(), command_pool_, 1, &res.buffer);
            }
        }
    }
    retired.clear();

    if (!swapchain_.acquire_next_image(device_.logical(), fs.image_available, &current_image_)) {
        swapchain_dirty_ = true;
        recreate_swapchain();
        return false;
    }

    VK_CHECK(vkResetFences(device_.logical(), 1, &fs.in_flight));
    in_frame_ = true;
    return true;
}

void GraphicsDeviceVulkan::end_frame() {
    if (!in_frame_) return;

    std::vector<VkCommandBuffer> buffers;
    buffers.reserve(frame_command_lists_.size());
    for (CommandListHandle h : frame_command_lists_) {
        auto* cl = command_lists_.get(h);
        if (cl == nullptr) continue;
        if (cl->recording) {
            ROVER_LOG_WARN("end_frame: command list {} not ended; ending now", h);
            VK_CHECK(vkEndCommandBuffer(cl->buffer));
            cl->recording = false;
        }
        buffers.push_back(cl->buffer);
        cl->submitted = true;
    }

    FrameSync& fs = frame_sync_[current_frame_];
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSemaphore render_finished_sem = swapchain_.render_finished_semaphore(current_image_);

    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &fs.image_available;
    submit.pWaitDstStageMask    = &wait_stage;
    submit.commandBufferCount   = static_cast<u32>(buffers.size());
    submit.pCommandBuffers      = buffers.empty() ? nullptr : buffers.data();
    submit.signalSemaphoreCount = render_finished_sem != VK_NULL_HANDLE ? 1u : 0u;
    submit.pSignalSemaphores    = render_finished_sem != VK_NULL_HANDLE ? &render_finished_sem : nullptr;

    VK_CHECK(vkQueueSubmit(device_.graphics_queue(), 1, &submit, fs.in_flight));

    in_frame_ = false;
}

void GraphicsDeviceVulkan::present() {
    VkSemaphore render_finished_sem = swapchain_.render_finished_semaphore(current_image_);
    const bool ok = swapchain_.present(device_.present_queue(), render_finished_sem, current_image_);
    if (!ok) {
        swapchain_dirty_ = true;
    }

    // Move command lists submitted this frame to the retired bucket for this
    // frame slot. They will be freed in begin_frame() of the next iteration
    // through this slot, after the fence guarantees GPU completion.
    auto& retired = retired_command_lists_[current_frame_];
    retired.insert(retired.end(), frame_command_lists_.begin(), frame_command_lists_.end());
    frame_command_lists_.clear();

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;

    if (swapchain_dirty_) {
        recreate_swapchain();
    }
}

void GraphicsDeviceVulkan::wait_idle() {
    if (device_.logical() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_.logical());
    }
}

TextureHandle GraphicsDeviceVulkan::get_swapchain_texture() {
    if (current_image_ >= swapchain_texture_handles_.size()) {
        return INVALID_HANDLE;
    }
    return swapchain_texture_handles_[current_image_];
}

u32 GraphicsDeviceVulkan::get_swapchain_image_count() {
    return static_cast<u32>(swapchain_texture_handles_.size());
}

TextureHandle GraphicsDeviceVulkan::get_swapchain_texture_at(u32 image_index) {
    if (image_index >= swapchain_texture_handles_.size()) {
        return INVALID_HANDLE;
    }
    return swapchain_texture_handles_[image_index];
}

Format GraphicsDeviceVulkan::get_swapchain_format() {
    return from_vk_format(swapchain_.image_format());
}

u32 GraphicsDeviceVulkan::get_swapchain_width() {
    return swapchain_.extent().width;
}

u32 GraphicsDeviceVulkan::get_swapchain_height() {
    return swapchain_.extent().height;
}

} // namespace rover
