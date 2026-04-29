#include "services/graphics/frame_graph.h"

#include "core/log/log.h"

#include <utility>

namespace rover
{

    // ---------------------------------------------------------------------------
    // PassBuilder
    // ---------------------------------------------------------------------------

    PassBuilder::PassBuilder(FrameGraph& graph, u32 pass_index) : graph_(graph), pass_index_(pass_index) {}

    void PassBuilder::read(RenderResourceId id)
    {
        if (id == INVALID_RESOURCE_ID)
        {
            return;
        }
        auto& passes = const_cast<std::vector<RenderPassNode>&>(graph_.passes());
        passes[pass_index_].reads.push_back(id);
    }

    void PassBuilder::write(RenderResourceId id)
    {
        if (id == INVALID_RESOURCE_ID)
        {
            return;
        }
        auto& passes = const_cast<std::vector<RenderPassNode>&>(graph_.passes());
        passes[pass_index_].writes.push_back(id);
    }

    // ---------------------------------------------------------------------------
    // FrameGraph
    // ---------------------------------------------------------------------------

    RenderResourceId FrameGraph::import_texture(std::string       name,
                                                TextureHandle     handle,
                                                Format            format,
                                                u32               width,
                                                u32               height,
                                                FramebufferHandle fb)
    {
        RenderResource res{};
        res.name        = std::move(name);
        res.kind        = ResourceKind::Texture;
        res.lifetime    = ResourceLifetime::Imported;
        res.texture     = handle;
        res.framebuffer = fb;
        res.width       = width;
        res.height      = height;
        res.format      = format;
        resources_.push_back(std::move(res));
        return static_cast<RenderResourceId>(resources_.size() - 1);
    }

    RenderResourceId FrameGraph::import_buffer(std::string name, BufferHandle handle)
    {
        RenderResource res{};
        res.name     = std::move(name);
        res.kind     = ResourceKind::Buffer;
        res.lifetime = ResourceLifetime::Imported;
        res.buffer   = handle;
        resources_.push_back(std::move(res));
        return static_cast<RenderResourceId>(resources_.size() - 1);
    }

    const RenderResource& FrameGraph::resource(RenderResourceId id) const
    {
        static const RenderResource invalid{};
        return id < resources_.size() ? resources_[id] : invalid;
    }

    RenderResource& FrameGraph::resource(RenderResourceId id)
    {
        static RenderResource invalid{};
        return id < resources_.size() ? resources_[id] : invalid;
    }

    u32 FrameGraph::add_pass(std::string name, PassSetupFn setup, PassExecuteFn execute)
    {
        RenderPassNode node{};
        node.name    = std::move(name);
        node.setup   = std::move(setup);
        node.execute = std::move(execute);
        passes_.push_back(std::move(node));
        const u32   idx = static_cast<u32>(passes_.size() - 1);
        PassBuilder builder{*this, idx};
        if (passes_[idx].setup)
        {
            passes_[idx].setup(builder);
        }
        return idx;
    }

    void FrameGraph::set_color_attachment(u32               pass_index,
                                          RenderResourceId  target,
                                          const ClearValue& clear,
                                          bool              clear_depth_stencil)
    {
        if (pass_index >= passes_.size() || target >= resources_.size())
        {
            return;
        }
        passes_[pass_index].framebuffer         = resources_[target].framebuffer;
        passes_[pass_index].has_clear           = true;
        passes_[pass_index].clear_depth_stencil = clear_depth_stencil;
        passes_[pass_index].clear               = clear;
    }

    bool FrameGraph::compile()
    {
        for (auto& pass : passes_)
        {
            for (auto id : pass.reads)
            {
                if (id >= resources_.size())
                {
                    ROVER_LOG_ERROR("FrameGraph: pass '{}' reads invalid resource", pass.name);
                    return false;
                }
            }
            for (auto id : pass.writes)
            {
                if (id >= resources_.size())
                {
                    ROVER_LOG_ERROR("FrameGraph: pass '{}' writes invalid resource", pass.name);
                    return false;
                }
            }
        }
        compiled_ = true;
        return true;
    }

    void FrameGraph::execute(GraphicsDevice& device)
    {
        if (!compiled_)
        {
            ROVER_LOG_WARN("FrameGraph::execute called before compile()");
            return;
        }
        for (auto& pass : passes_)
        {
            const CommandListHandle cmd = device.begin_command_list();
            if (cmd == INVALID_HANDLE)
            {
                ROVER_LOG_WARN("FrameGraph: failed to open command list for pass '{}'", pass.name);
                continue;
            }

            // If the pass declares a color attachment, automatically open/close
            // a render pass around the user's execute callback.
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
                else
                {
                    device.cmd_begin_render_pass(cmd, pass.framebuffer, nullptr, 0u);
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
    }

    void FrameGraph::reset()
    {
        resources_.clear();
        passes_.clear();
        compiled_ = false;
    }

} // namespace rover
