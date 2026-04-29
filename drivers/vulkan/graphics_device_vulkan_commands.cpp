#include "core/log/log.h"
#include "drivers/vulkan/graphics_device_vulkan.h"
#include "drivers/vulkan/vk_format.h"

#include <vector>

namespace rover
{

    CommandListHandle GraphicsDeviceVulkan::begin_command_list()
    {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool        = command_pool_;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(device_.logical(), &alloc_info, &cmd) != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("vkAllocateCommandBuffers failed");
            return INVALID_HANDLE;
        }

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("vkBeginCommandBuffer failed");
            vkFreeCommandBuffers(device_.logical(), command_pool_, 1, &cmd);
            return INVALID_HANDLE;
        }

        VkCommandListResource res{};
        res.buffer                     = cmd;
        res.recording                  = true;
        res.submitted                  = false;
        const CommandListHandle handle = command_lists_.add(res);
        frame_command_lists_.push_back(handle);
        return handle;
    }

    void GraphicsDeviceVulkan::end_command_list(CommandListHandle cmd)
    {
        auto* res = command_lists_.get(cmd);
        if (res == nullptr || !res->recording)
        {
            return;
        }
        if (vkEndCommandBuffer(res->buffer) != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("vkEndCommandBuffer failed");
        }
        res->recording = false;
    }

    void GraphicsDeviceVulkan::cmd_begin_render_pass(CommandListHandle cmd,
                                                     FramebufferHandle fb,
                                                     const ClearValue* clear_values,
                                                     u32               clear_count)
    {
        auto* cl     = command_lists_.get(cmd);
        auto* fb_res = framebuffers_.get(fb);
        if (cl == nullptr || fb_res == nullptr)
        {
            return;
        }
        auto* rp = render_passes_.get(fb_res->render_pass);
        if (rp == nullptr)
        {
            ROVER_LOG_ERROR("cmd_begin_render_pass: framebuffer has invalid render_pass");
            return;
        }

        std::vector<VkClearValue> vk_clears(clear_count);
        for (u32 i = 0; i < clear_count; ++i)
        {
            const ClearValue& cv       = clear_values[i];
            const bool        is_depth = rp->has_depth && i + 1 == clear_count && i >= rp->color_formats.size();
            if (is_depth)
            {
                vk_clears[i].depthStencil.depth   = cv.depth;
                vk_clears[i].depthStencil.stencil = cv.stencil;
            }
            else
            {
                vk_clears[i].color.float32[0] = cv.color[0];
                vk_clears[i].color.float32[1] = cv.color[1];
                vk_clears[i].color.float32[2] = cv.color[2];
                vk_clears[i].color.float32[3] = cv.color[3];
            }
        }

        VkRenderPassBeginInfo info{};
        info.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass        = rp->pass;
        info.framebuffer       = fb_res->framebuffer;
        info.renderArea.offset = {0, 0};
        info.renderArea.extent = {fb_res->width, fb_res->height};
        info.clearValueCount   = clear_count;
        info.pClearValues      = vk_clears.empty() ? nullptr : vk_clears.data();

        vkCmdBeginRenderPass(cl->buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    void GraphicsDeviceVulkan::cmd_end_render_pass(CommandListHandle cmd)
    {
        auto* cl = command_lists_.get(cmd);
        if (cl == nullptr)
        {
            return;
        }
        vkCmdEndRenderPass(cl->buffer);
    }

    void GraphicsDeviceVulkan::cmd_bind_pipeline(CommandListHandle cmd, PipelineHandle pipeline)
    {
        auto* cl = command_lists_.get(cmd);
        auto* p  = pipelines_.get(pipeline);
        if (cl == nullptr || p == nullptr)
        {
            return;
        }
        vkCmdBindPipeline(cl->buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p->pipeline);

        // Capture the pipeline's layout so cmd_bind_group / cmd_push_constants
        // can route their calls without re-querying the pipeline.
        if (p->layout != INVALID_HANDLE)
        {
            if (auto* pl = pipeline_layouts_.get(p->layout))
            {
                cl->current_layout = pl->layout;
            }
        }
    }

    void GraphicsDeviceVulkan::cmd_set_viewport(CommandListHandle cmd, const Viewport& viewport)
    {
        auto* cl = command_lists_.get(cmd);
        if (cl == nullptr)
        {
            return;
        }
        VkViewport vp{};
        vp.x        = viewport.x;
        vp.y        = viewport.y;
        vp.width    = viewport.width;
        vp.height   = viewport.height;
        vp.minDepth = viewport.min_depth;
        vp.maxDepth = viewport.max_depth;
        vkCmdSetViewport(cl->buffer, 0, 1, &vp);
    }

    void GraphicsDeviceVulkan::cmd_set_scissor(CommandListHandle cmd, const Scissor& scissor)
    {
        auto* cl = command_lists_.get(cmd);
        if (cl == nullptr)
        {
            return;
        }
        VkRect2D r{};
        r.offset.x      = scissor.x;
        r.offset.y      = scissor.y;
        r.extent.width  = scissor.width;
        r.extent.height = scissor.height;
        vkCmdSetScissor(cl->buffer, 0, 1, &r);
    }

    void GraphicsDeviceVulkan::cmd_bind_vertex_buffer(CommandListHandle cmd,
                                                      BufferHandle      buffer,
                                                      u32               binding,
                                                      usize             offset)
    {
        auto* cl  = command_lists_.get(cmd);
        auto* buf = buffers_.get(buffer);
        if (cl == nullptr || buf == nullptr)
        {
            return;
        }
        VkDeviceSize off = offset;
        vkCmdBindVertexBuffers(cl->buffer, binding, 1, &buf->buffer, &off);
    }

    void GraphicsDeviceVulkan::cmd_bind_index_buffer(CommandListHandle cmd,
                                                     BufferHandle      buffer,
                                                     IndexType         type,
                                                     usize             offset)
    {
        auto* cl  = command_lists_.get(cmd);
        auto* buf = buffers_.get(buffer);
        if (cl == nullptr || buf == nullptr)
        {
            return;
        }
        vkCmdBindIndexBuffer(cl->buffer, buf->buffer, offset, to_vk_index_type(type));
    }

    void GraphicsDeviceVulkan::cmd_draw(CommandListHandle cmd,
                                        u32               vertex_count,
                                        u32               instance_count,
                                        u32               first_vertex,
                                        u32               first_instance)
    {
        auto* cl = command_lists_.get(cmd);
        if (cl == nullptr)
        {
            return;
        }
        vkCmdDraw(cl->buffer, vertex_count, instance_count, first_vertex, first_instance);
    }

    void GraphicsDeviceVulkan::cmd_draw_indexed(CommandListHandle cmd,
                                                u32               index_count,
                                                u32               instance_count,
                                                u32               first_index,
                                                i32               vertex_offset,
                                                u32               first_instance)
    {
        auto* cl = command_lists_.get(cmd);
        if (cl == nullptr)
        {
            return;
        }
        vkCmdDrawIndexed(cl->buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
    }

} // namespace rover
