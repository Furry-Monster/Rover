#include "services/graphics/frame_graph.h"

#include "core/log/log.h"
#include "services/graphics/transient_resource_pool.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <unordered_map>
#include <utility>

namespace rover
{

    PassBuilder::PassBuilder(FrameGraph& graph, u32 pass_index) : graph_(graph), pass_index_(pass_index) {}

    void PassBuilder::read(RenderResourceId id, ResourceAccess access, TextureSubresourceRange range)
    {
        graph_.declare_access(pass_index_, id, access, false, range);
    }

    void PassBuilder::write(RenderResourceId id, ResourceAccess access, TextureSubresourceRange range)
    {
        graph_.declare_access(pass_index_, id, access, true, range);
    }

    void FrameGraph::invalidate()
    {
        compiled_        = false;
        prepared_serial_ = 0;
        prepared_device_ = nullptr;
        prepared_pool_   = nullptr;
        execution_order_.clear();
        dependencies_.clear();
        barriers_.clear();
        final_barriers_.clear();
    }

    bool FrameGraph::prepare_mutation()
    {
        if (executing_)
        {
            ROVER_LOG_ERROR("FrameGraph: cannot modify the graph during execution");
            return false;
        }
        invalidate();
        return true;
    }

    void FrameGraph::declare_access(u32                     pass_index,
                                    RenderResourceId        id,
                                    ResourceAccess          access,
                                    bool                    write,
                                    TextureSubresourceRange range)
    {
        if (!prepare_mutation())
        {
            return;
        }
        if (pass_index >= passes_.size())
        {
            ROVER_LOG_ERROR("FrameGraph: access declaration for invalid pass {}", pass_index);
            declaration_error_ = true;
            return;
        }
        auto& accesses = write ? passes_[pass_index].writes : passes_[pass_index].reads;
        if (std::find(accesses.begin(), accesses.end(), id) == accesses.end())
        {
            accesses.push_back(id);
        }
        auto&      uses      = passes_[pass_index].accesses;
        const auto duplicate = std::find_if(uses.begin(), uses.end(), [&](const auto& use) {
            return use.resource == id && use.usage == access && use.write == write && use.range == range;
        });
        if (duplicate == uses.end())
        {
            uses.push_back({id, access, write, range});
        }
    }

    RenderResourceId FrameGraph::import_texture(std::string       name,
                                                TextureHandle     handle,
                                                Format            format,
                                                u32               width,
                                                u32               height,
                                                FramebufferHandle fb)
    {
        if (!prepare_mutation())
        {
            return INVALID_RESOURCE_ID;
        }
        RenderResource res{};
        res.name                = std::move(name);
        res.kind                = ResourceKind::Texture;
        res.lifetime            = ResourceLifetime::Imported;
        res.texture             = handle;
        res.framebuffer         = fb;
        res.width               = width;
        res.height              = height;
        res.format              = format;
        res.texture_desc.width  = width;
        res.texture_desc.height = height;
        res.texture_desc.format = format;
        res.physical_resource   = static_cast<RenderResourceId>(resources_.size());
        resources_.push_back(std::move(res));
        return static_cast<RenderResourceId>(resources_.size() - 1);
    }

    RenderResourceId FrameGraph::import_texture(std::string        name,
                                                TextureHandle      handle,
                                                const TextureDesc& desc,
                                                FramebufferHandle  fb)
    {
        const auto id = import_texture(std::move(name), handle, desc.format, desc.width, desc.height, fb);
        if (id != INVALID_RESOURCE_ID)
        {
            resources_[id].texture_desc = desc;
        }
        return id;
    }

    RenderResourceId FrameGraph::import_buffer(std::string name, BufferHandle handle)
    {
        if (!prepare_mutation())
        {
            return INVALID_RESOURCE_ID;
        }
        RenderResource res{};
        res.name              = std::move(name);
        res.kind              = ResourceKind::Buffer;
        res.lifetime          = ResourceLifetime::Imported;
        res.buffer            = handle;
        res.physical_resource = static_cast<RenderResourceId>(resources_.size());
        resources_.push_back(std::move(res));
        return static_cast<RenderResourceId>(resources_.size() - 1);
    }

    RenderResourceId FrameGraph::create_texture(std::string name, const TextureDesc& desc)
    {
        const auto id = import_texture(std::move(name), INVALID_HANDLE, desc.format, desc.width, desc.height);
        if (id != INVALID_RESOURCE_ID)
        {
            resources_[id].lifetime                = ResourceLifetime::Transient;
            resources_[id].texture_desc            = desc;
            resources_[id].texture_desc.debug_name = nullptr;
        }
        return id;
    }

    RenderResourceId FrameGraph::create_buffer(std::string name, const BufferDesc& desc)
    {
        const auto id = import_buffer(std::move(name), INVALID_HANDLE);
        if (id != INVALID_RESOURCE_ID)
        {
            resources_[id].lifetime               = ResourceLifetime::Transient;
            resources_[id].buffer_desc            = desc;
            resources_[id].buffer_desc.debug_name = nullptr;
        }
        return id;
    }

    RenderBindGroupId FrameGraph::create_bind_group(BindGroupLayoutHandle layout, std::vector<RenderBinding> entries)
    {
        if (!prepare_mutation())
        {
            return INVALID_BIND_GROUP_ID;
        }
        bind_groups_.push_back({layout, std::move(entries), INVALID_HANDLE});
        return static_cast<RenderBindGroupId>(bind_groups_.size() - 1);
    }

    BindGroupHandle FrameGraph::bind_group(RenderBindGroupId id) const
    {
        return id < bind_groups_.size() ? bind_groups_[id].handle : INVALID_HANDLE;
    }

    RenderResourceId FrameGraph::create_version(RenderResourceId previous, std::string name)
    {
        if (!prepare_mutation())
        {
            return INVALID_RESOURCE_ID;
        }
        if (previous >= resources_.size())
        {
            ROVER_LOG_ERROR("FrameGraph: version declaration for invalid resource {}", previous);
            declaration_error_ = true;
            return INVALID_RESOURCE_ID;
        }
        auto       version = resources_[previous];
        const auto id      = static_cast<RenderResourceId>(resources_.size());
        version.name =
            name.empty() ? resources_[version.physical_resource].name + "#" + std::to_string(id) : std::move(name);
        version.previous_version = previous;
        version.final_access     = ResourceAccess::Undefined;
        version.exports.clear();
        resources_.push_back(std::move(version));
        return id;
    }

    const RenderResource& FrameGraph::resource(RenderResourceId id) const
    {
        static const RenderResource invalid{};
        return id < resources_.size() ? resources_[id] : invalid;
    }

    void FrameGraph::set_final_access(RenderResourceId id, ResourceAccess access, TextureSubresourceRange range)
    {
        if (!prepare_mutation())
        {
            return;
        }
        if (id >= resources_.size())
        {
            ROVER_LOG_ERROR("FrameGraph: final access declaration for invalid resource {}", id);
            declaration_error_ = true;
            return;
        }
        auto& resource = resources_[id];
        if (range == TextureSubresourceRange{})
        {
            resource.final_access = access;
            resource.exports.clear();
        }
        else
        {
            auto found = std::find_if(
                resource.exports.begin(), resource.exports.end(), [&](const auto& e) { return e.range == range; });
            if (found != resource.exports.end())
            {
                resource.exports.erase(found);
            }
            if (access != ResourceAccess::Undefined)
            {
                resource.exports.push_back({access, range});
            }
        }
    }

    u32 FrameGraph::add_pass(std::string name, PassSetupFn setup, PassExecuteFn execute)
    {
        if (!prepare_mutation())
        {
            return INVALID_PASS_ID;
        }
        RenderPassNode node{};
        node.name    = std::move(name);
        node.setup   = setup;
        node.execute = std::move(execute);
        passes_.push_back(std::move(node));
        const u32   idx = static_cast<u32>(passes_.size() - 1);
        PassBuilder builder{*this, idx};
        // Invoke the local callback: nested registration may reallocate passes_.
        if (setup)
        {
            setup(builder);
        }
        return idx;
    }

    void FrameGraph::set_color_attachment(u32               pass_index,
                                          RenderResourceId  target,
                                          const ClearValue& clear,
                                          bool              clear_depth_stencil)
    {
        if (!prepare_mutation())
        {
            return;
        }
        if (pass_index >= passes_.size())
        {
            ROVER_LOG_ERROR("FrameGraph: attachment declaration for invalid pass {}", pass_index);
            declaration_error_ = true;
            return;
        }
        auto& pass            = passes_[pass_index];
        pass.color_attachment = target;
        pass.framebuffer      = target < resources_.size() ? resources_[target].framebuffer : INVALID_HANDLE;
        pass.render_pass      = INVALID_HANDLE;
        pass.depth_attachment = INVALID_RESOURCE_ID;
        pass.color_range = pass.depth_range = TEXTURE_BASE_SUBRESOURCE;
        pass.has_clear                      = true;
        pass.clear_depth_stencil            = clear_depth_stencil;
        pass.clear                          = clear;
    }

    void FrameGraph::set_render_target(u32                     pass_index,
                                       RenderPassHandle        render_pass,
                                       RenderResourceId        color,
                                       const ClearValue&       clear,
                                       RenderResourceId        depth,
                                       TextureSubresourceRange color_range,
                                       TextureSubresourceRange depth_range)
    {
        set_color_attachment(pass_index, color, clear, depth != INVALID_RESOURCE_ID);
        if (!executing_ && pass_index < passes_.size())
        {
            passes_[pass_index].render_pass      = render_pass;
            passes_[pass_index].depth_attachment = depth;
            passes_[pass_index].color_range      = color_range;
            passes_[pass_index].depth_range      = depth_range;
            if (!render_pass)
            {
                declaration_error_ = true;
            }
        }
    }

    bool FrameGraph::prepare_resources(TransientResourcePool& pool)
    {
        return pool.prepare(*this);
    }

    bool FrameGraph::execute(GraphicsDevice& device)
    {
        if (executing_)
        {
            ROVER_LOG_ERROR("FrameGraph: recursive execution is unsupported");
            return false;
        }
        if (!compiled_)
        {
            ROVER_LOG_WARN("FrameGraph::execute requires a successful compile() after graph changes");
            return false;
        }
        const bool needs_pool =
            !bind_groups_.empty() ||
            std::any_of(resources_.begin(),
                        resources_.end(),
                        [](const auto& r) { return r.lifetime == ResourceLifetime::Transient; }) ||
            std::any_of(passes_.begin(), passes_.end(), [](const auto& p) { return p.render_pass != INVALID_HANDLE; });
        if (needs_pool &&
            (prepared_device_ != &device || prepared_serial_ == 0 || prepared_serial_ != device.recording_serial() ||
             !prepared_pool_ || prepared_epoch_ != prepared_pool_->generation_))
        {
            ROVER_LOG_ERROR("FrameGraph: prepare resources for the current recording submission before execution");
            return false;
        }
        if (needs_pool)
        {
            prepared_pool_->recorded_serial_ = prepared_serial_;
        }
        executing_     = true;
        bool succeeded = true;
        for (auto pass_id : execution_order_)
        {
            const auto&             pass = passes_[pass_id];
            const CommandListHandle cmd  = device.begin_command_list();
            if (cmd == INVALID_HANDLE)
            {
                ROVER_LOG_WARN("FrameGraph: failed to open command list for pass '{}'; stopping execution", pass.name);
                succeeded = false;
                break;
            }

            if (!record_barriers(device, cmd, barriers_[pass_id]))
            {
                ROVER_LOG_ERROR("FrameGraph: failed to synchronize pass '{}'; stopping execution", pass.name);
                device.end_command_list(cmd);
                succeeded = false;
                break;
            }

            const bool wraps_render_pass = pass.framebuffer != INVALID_HANDLE;
            if (wraps_render_pass)
            {
                if (pass.has_clear && pass.clear_depth_stencil)
                {
                    const ClearValue clears[2] = {pass.clear, pass.clear};
                    device.cmd_begin_render_pass(cmd, pass.framebuffer, clears, 2u);
                }
                else if (pass.has_clear)
                {
                    const ClearValue clears[1] = {pass.clear};
                    device.cmd_begin_render_pass(cmd, pass.framebuffer, clears, 1u);
                }
            }
            if (pass.execute)
            {
                PassExecuteContext ctx{*this, device, cmd};
                pass.execute(ctx);
            }
            if (wraps_render_pass)
            {
                device.cmd_end_render_pass(cmd);
            }
            device.end_command_list(cmd);
        }
        if (succeeded && !final_barriers_.empty())
        {
            const auto cmd = device.begin_command_list();
            if (cmd != INVALID_HANDLE)
            {
                if (!record_barriers(device, cmd, final_barriers_))
                {
                    ROVER_LOG_ERROR("FrameGraph: failed to synchronize exported resources");
                    succeeded = false;
                }
                device.end_command_list(cmd);
            }
            else
            {
                ROVER_LOG_ERROR("FrameGraph: failed to open export command list");
                succeeded = false;
            }
        }
        executing_ = false;
        return succeeded;
    }

    bool FrameGraph::record_barriers(GraphicsDevice&                           device,
                                     CommandListHandle                         cmd,
                                     const std::vector<RenderResourceBarrier>& barriers) const
    {
        if (barriers.empty())
        {
            return true;
        }
        std::vector<BufferBarrier>  buffers;
        std::vector<TextureBarrier> textures;
        for (const auto& barrier : barriers)
        {
            const auto& res = resources_[barrier.resource];
            if (res.kind == ResourceKind::Texture)
            {
                textures.push_back({res.texture, barrier.after, barrier.range});
            }
            else
            {
                buffers.push_back({res.buffer, barrier.after});
            }
        }
        return device.cmd_resource_barriers(cmd, buffers, textures);
    }

    void FrameGraph::reset()
    {
        if (!prepare_mutation())
        {
            return;
        }
        resources_.clear();
        passes_.clear();
        bind_groups_.clear();
        declaration_error_ = false;
    }

} // namespace rover
