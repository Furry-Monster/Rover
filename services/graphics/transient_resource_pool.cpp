#include "services/graphics/transient_resource_pool.h"

#include "core/log/log.h"
#include "services/graphics/frame_graph.h"

#include <algorithm>
#include <numeric>
#include <utility>

namespace rover
{
    namespace
    {
        bool same(const TextureDesc& a, const TextureDesc& b)
        {
            return a.width == b.width && a.height == b.height && a.depth == b.depth && a.mip_levels == b.mip_levels &&
                   a.array_layers == b.array_layers && a.format == b.format && a.type == b.type && a.usage == b.usage;
        }

        bool same(const BufferDesc& a, const BufferDesc& b)
        {
            return a.size == b.size && a.usage == b.usage && a.memory == b.memory;
        }

        bool same(const FramebufferDesc& a, const FramebufferDesc& b)
        {
            return a.render_pass == b.render_pass && a.color_attachments == b.color_attachments &&
                   a.depth_stencil == b.depth_stencil && a.width == b.width && a.height == b.height;
        }

        bool same(const BindGroupDesc& a, const BindGroupDesc& b)
        {
            if (a.layout != b.layout || a.entries.size() != b.entries.size())
            {
                return false;
            }
            for (usize i = 0; i < a.entries.size(); ++i)
            {
                const auto& x = a.entries[i];
                const auto& y = b.entries[i];
                if (x.binding != y.binding || x.type != y.type || x.buffer != y.buffer ||
                    x.buffer_offset != y.buffer_offset || x.buffer_size != y.buffer_size || x.texture != y.texture ||
                    x.sampler != y.sampler)
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace

    TransientResourcePool::~TransientResourcePool()
    {
        clear();
    }

    bool TransientResourcePool::prepare(FrameGraph& graph)
    {
        const auto serial = device_.recording_serial();
        if (!graph.compiled_ || graph.executing_ || serial == 0)
        {
            ROVER_LOG_ERROR("TransientResourcePool: requires a compiled graph inside a frame");
            return false;
        }
        if (graph.prepared_pool_ == this && graph.prepared_serial_ == serial && graph.prepared_epoch_ == generation_)
        {
            return true;
        }
        const auto       completed    = device_.completed_serial();
        auto             textures     = textures_;
        auto             buffers      = buffers_;
        auto             framebuffers = framebuffers_;
        auto             bindings     = bindings_;
        auto             resources    = graph.resources_;
        auto             passes       = graph.passes_;
        auto             groups       = graph.bind_groups_;
        std::vector<u32> texture_end(textures.size(), INVALID_PASS_ID);
        std::vector<u32> buffer_end(buffers.size(), INVALID_PASS_ID);
        auto             fail = [&]() {
            for (usize i = bindings_.size(); i < bindings.size(); ++i)
            {
                device_.destroy_bind_group(bindings[i].handle);
            }
            for (usize i = framebuffers_.size(); i < framebuffers.size(); ++i)
            {
                device_.destroy_framebuffer(framebuffers[i].handle);
            }
            for (usize i = textures_.size(); i < textures.size(); ++i)
            {
                device_.destroy_texture(textures[i].handle);
            }
            for (usize i = buffers_.size(); i < buffers.size(); ++i)
            {
                device_.destroy_buffer(buffers[i].handle);
            }
            ROVER_LOG_ERROR("TransientResourcePool: resource preparation failed; candidates released");
            return false;
        };
        std::vector<RenderResourceId> order(resources.size());
        std::iota(order.begin(), order.end(), 0u);
        std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
            return resources[a].first_use < resources[b].first_use;
        });
        for (auto id : order)
        {
            auto& resource = resources[id];
            if (resource.physical_resource != id || resource.lifetime != ResourceLifetime::Transient ||
                resource.first_use == INVALID_PASS_ID)
            {
                continue;
            }
            if (resource.kind == ResourceKind::Texture)
            {
                usize slot = 0;
                for (; slot < textures.size(); ++slot)
                {
                    const bool available = texture_end[slot] == INVALID_PASS_ID
                                               ? textures[slot].retired <= completed
                                               : texture_end[slot] < resource.first_use;
                    if (available && same(textures[slot].desc, resource.texture_desc))
                    {
                        break;
                    }
                }
                if (slot == textures.size())
                {
                    auto desc         = resource.texture_desc;
                    desc.debug_name   = nullptr;
                    const auto handle = device_.create_texture(desc);
                    if (!handle)
                    {
                        return fail();
                    }
                    textures.push_back({desc, handle, 0});
                    texture_end.push_back(INVALID_PASS_ID);
                }
                auto& entry       = textures[slot];
                entry.retired     = serial;
                texture_end[slot] = resource.last_use;
                resource.texture  = entry.handle;
            }
            else
            {
                usize slot = 0;
                for (; slot < buffers.size(); ++slot)
                {
                    const bool available = buffer_end[slot] == INVALID_PASS_ID
                                               ? buffers[slot].retired <= completed
                                               : resource.buffer_desc.memory == MemoryUsage::GpuOnly &&
                                                     buffer_end[slot] < resource.first_use;
                    if (available && same(buffers[slot].desc, resource.buffer_desc))
                    {
                        break;
                    }
                }
                if (slot == buffers.size())
                {
                    auto desc         = resource.buffer_desc;
                    desc.debug_name   = nullptr;
                    const auto handle = device_.create_buffer(desc);
                    if (!handle)
                    {
                        return fail();
                    }
                    buffers.push_back({desc, handle, 0});
                    buffer_end.push_back(INVALID_PASS_ID);
                }
                auto& entry      = buffers[slot];
                entry.retired    = serial;
                buffer_end[slot] = resource.last_use;
                resource.buffer  = entry.handle;
            }
        }
        for (auto& resource : resources)
        {
            const auto& root = resources[resource.physical_resource];
            resource.texture = root.texture;
            resource.buffer  = root.buffer;
        }
        for (auto& pass : passes)
        {
            if (!pass.render_pass)
            {
                continue;
            }
            const auto&     color = resources[pass.color_attachment];
            FramebufferDesc desc{};
            desc.render_pass       = pass.render_pass;
            desc.color_attachments = {color.texture};
            desc.depth_stencil     = pass.depth_attachment == INVALID_RESOURCE_ID
                                         ? INVALID_HANDLE
                                         : resources[pass.depth_attachment].texture;
            desc.width             = color.width;
            desc.height            = color.height;
            auto found             = std::find_if(
                framebuffers.begin(), framebuffers.end(), [&](const auto& entry) { return same(entry.desc, desc); });
            if (found == framebuffers.end())
            {
                const auto handle = device_.create_framebuffer(desc);
                if (!handle)
                {
                    return fail();
                }
                framebuffers.push_back({desc, handle});
                pass.framebuffer = handle;
            }
            else
            {
                pass.framebuffer = found->handle;
            }
        }
        for (auto& group : groups)
        {
            BindGroupDesc desc{};
            desc.layout = group.layout;
            for (auto binding : group.entries)
            {
                if (binding.resource != INVALID_RESOURCE_ID)
                {
                    const auto& resource  = resources[binding.resource];
                    binding.entry.texture = resource.texture;
                    binding.entry.buffer  = resource.buffer;
                }
                desc.entries.push_back(binding.entry);
            }
            auto found = std::find_if(
                bindings.begin(), bindings.end(), [&](const auto& entry) { return same(entry.desc, desc); });
            if (found == bindings.end())
            {
                const auto handle = device_.create_bind_group(desc);
                if (!handle)
                {
                    return fail();
                }
                bindings.push_back({std::move(desc), handle});
                group.handle = handle;
            }
            else
            {
                group.handle = found->handle;
            }
        }
        textures_              = std::move(textures);
        buffers_               = std::move(buffers);
        framebuffers_          = std::move(framebuffers);
        bindings_              = std::move(bindings);
        graph.resources_       = std::move(resources);
        graph.passes_          = std::move(passes);
        graph.bind_groups_     = std::move(groups);
        graph.prepared_device_ = &device_;
        graph.prepared_pool_   = this;
        graph.prepared_epoch_  = generation_;
        graph.prepared_serial_ = serial;
        return true;
    }

    bool TransientResourcePool::clear()
    {
        if (textures_.empty() && buffers_.empty() && framebuffers_.empty() && bindings_.empty())
        {
            return true;
        }
        if (recorded_serial_ != 0 && device_.recording_serial() == recorded_serial_)
        {
            ROVER_LOG_ERROR("TransientResourcePool: submit the recorded frame before clearing resources");
            return false;
        }
        device_.wait_idle();
        ++generation_;
        ROVER_LOG_INFO("TransientResourcePool: releasing {} textures, {} buffers, {} framebuffers, {} bindings",
                       textures_.size(),
                       buffers_.size(),
                       framebuffers_.size(),
                       bindings_.size());
        for (const auto& entry : bindings_)
        {
            device_.destroy_bind_group(entry.handle);
        }
        for (const auto& entry : framebuffers_)
        {
            device_.destroy_framebuffer(entry.handle);
        }
        for (const auto& entry : textures_)
        {
            device_.destroy_texture(entry.handle);
        }
        for (const auto& entry : buffers_)
        {
            device_.destroy_buffer(entry.handle);
        }
        recorded_serial_ = 0;
        bindings_.clear();
        framebuffers_.clear();
        textures_.clear();
        buffers_.clear();
        return true;
    }
} // namespace rover
