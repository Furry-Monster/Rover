#include "services/graphics/frame_graph.h"

#include "core/log/log.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <unordered_map>
#include <utility>

namespace rover
{

    PassBuilder::PassBuilder(FrameGraph& graph, u32 pass_index) : graph_(graph), pass_index_(pass_index) {}

    void PassBuilder::read(RenderResourceId id)
    {
        graph_.declare_access(pass_index_, id, false);
    }

    void PassBuilder::write(RenderResourceId id)
    {
        graph_.declare_access(pass_index_, id, true);
    }

    void FrameGraph::invalidate()
    {
        compiled_ = false;
        execution_order_.clear();
        dependencies_.clear();
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

    void FrameGraph::declare_access(u32 pass_index, RenderResourceId id, bool write)
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
        if (!prepare_mutation())
        {
            return INVALID_RESOURCE_ID;
        }
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
        auto& pass               = passes_[pass_index];
        pass.color_attachment    = target;
        pass.framebuffer         = target < resources_.size() ? resources_[target].framebuffer : INVALID_HANDLE;
        pass.has_clear           = true;
        pass.clear_depth_stencil = clear_depth_stencil;
        pass.clear               = clear;
    }

    bool FrameGraph::compile()
    {
        if (executing_)
        {
            ROVER_LOG_ERROR("FrameGraph: cannot compile during execution");
            return false;
        }
        invalidate();
        if (declaration_error_)
        {
            ROVER_LOG_ERROR("FrameGraph: invalid pass declarations; reset the graph before rebuilding");
            return false;
        }

        // Aliased imports could hide dependencies behind distinct resource IDs.
        std::unordered_map<u64, RenderResourceId> textures;
        std::unordered_map<u64, RenderResourceId> buffers;
        for (RenderResourceId id = 0; id < resources_.size(); ++id)
        {
            const auto& res    = resources_[id];
            const auto  handle = res.kind == ResourceKind::Texture ? res.texture : res.buffer;
            if (handle == INVALID_HANDLE)
            {
                ROVER_LOG_ERROR("FrameGraph: resource '{}' has an invalid imported handle", res.name);
                return false;
            }
            auto& imports                   = res.kind == ResourceKind::Texture ? textures : buffers;
            const auto [previous, inserted] = imports.emplace(handle, id);
            if (!inserted)
            {
                ROVER_LOG_ERROR("FrameGraph: resources '{}' and '{}' alias the same imported handle",
                                resources_[previous->second].name,
                                res.name);
                return false;
            }
        }

        std::vector<u32> writers(resources_.size(), INVALID_PASS_ID);
        auto             record_write = [&](u32 pass_id, RenderResourceId id) {
            if (id >= resources_.size())
            {
                ROVER_LOG_ERROR("FrameGraph: pass '{}' writes invalid resource {}", passes_[pass_id].name, id);
                return false;
            }
            if (writers[id] != INVALID_PASS_ID && writers[id] != pass_id)
            {
                ROVER_LOG_ERROR("FrameGraph: resource '{}' has multiple writers: '{}' and '{}'",
                                resources_[id].name,
                                passes_[writers[id]].name,
                                passes_[pass_id].name);
                return false;
            }
            writers[id] = pass_id;
            return true;
        };

        // Gather every producer first, including those registered after readers.
        for (u32 pass_id = 0; pass_id < passes_.size(); ++pass_id)
        {
            const auto& pass = passes_[pass_id];
            for (auto id : pass.writes)
            {
                if (!record_write(pass_id, id))
                {
                    return false;
                }
            }
            if (pass.has_clear)
            {
                const auto target = pass.color_attachment;
                if (target >= resources_.size() || resources_[target].kind != ResourceKind::Texture ||
                    resources_[target].framebuffer == INVALID_HANDLE)
                {
                    ROVER_LOG_ERROR("FrameGraph: pass '{}' has an invalid color attachment {}", pass.name, target);
                    return false;
                }
                if (!record_write(pass_id, target))
                {
                    return false;
                }
            }
        }

        std::vector<std::vector<u32>> dependencies(passes_.size());
        std::vector<std::vector<u32>> consumers(passes_.size());
        std::vector<usize>            indegrees(passes_.size(), 0);
        for (u32 pass_id = 0; pass_id < passes_.size(); ++pass_id)
        {
            const auto& pass          = passes_[pass_id];
            auto&       prerequisites = dependencies[pass_id];
            for (auto id : pass.reads)
            {
                if (id >= resources_.size())
                {
                    ROVER_LOG_ERROR("FrameGraph: pass '{}' reads invalid resource {}", pass.name, id);
                    return false;
                }
                const auto writer = writers[id];
                if (writer == pass_id)
                {
                    ROVER_LOG_ERROR("FrameGraph: pass '{}' reads and writes resource '{}' (feedback unsupported)",
                                    pass.name,
                                    resources_[id].name);
                    return false;
                }
                if (writer != INVALID_PASS_ID)
                {
                    prerequisites.push_back(writer);
                }
            }
            std::sort(prerequisites.begin(), prerequisites.end());
            prerequisites.erase(std::unique(prerequisites.begin(), prerequisites.end()), prerequisites.end());
            indegrees[pass_id] = prerequisites.size();
            for (auto prerequisite : prerequisites)
            {
                consumers[prerequisite].push_back(pass_id);
            }
        }

        // Kahn's algorithm with registration IDs as a deterministic tie breaker.
        std::priority_queue<u32, std::vector<u32>, std::greater<u32>> ready;
        for (u32 pass_id = 0; pass_id < passes_.size(); ++pass_id)
        {
            if (indegrees[pass_id] == 0)
            {
                ready.push(pass_id);
            }
        }
        std::vector<u32> order;
        order.reserve(passes_.size());
        while (!ready.empty())
        {
            const auto pass_id = ready.top();
            ready.pop();
            order.push_back(pass_id);
            for (auto consumer : consumers[pass_id])
            {
                if (--indegrees[consumer] == 0)
                {
                    ready.push(consumer);
                }
            }
        }
        if (order.size() != passes_.size())
        {
            for (u32 pass_id = 0; pass_id < passes_.size(); ++pass_id)
            {
                if (indegrees[pass_id] != 0)
                {
                    ROVER_LOG_ERROR("FrameGraph: pass '{}' is blocked by cyclic dependencies", passes_[pass_id].name);
                }
            }
            return false;
        }
        dependencies_    = std::move(dependencies);
        execution_order_ = std::move(order);
        compiled_        = true;
        return true;
    }

    void FrameGraph::execute(GraphicsDevice& device)
    {
        if (executing_)
        {
            ROVER_LOG_ERROR("FrameGraph: recursive execution is unsupported");
            return;
        }
        if (!compiled_)
        {
            ROVER_LOG_WARN("FrameGraph::execute requires a successful compile() after graph changes");
            return;
        }
        executing_ = true;
        for (auto pass_id : execution_order_)
        {
            const auto&             pass = passes_[pass_id];
            const CommandListHandle cmd  = device.begin_command_list();
            if (cmd == INVALID_HANDLE)
            {
                ROVER_LOG_WARN("FrameGraph: failed to open command list for pass '{}'; stopping execution", pass.name);
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
        executing_ = false;
    }

    void FrameGraph::reset()
    {
        if (!prepare_mutation())
        {
            return;
        }
        resources_.clear();
        passes_.clear();
        declaration_error_ = false;
    }

} // namespace rover
