#include "services/graphics/frame_graph.h"

#include "core/log/log.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <unordered_map>
#include <utility>

namespace rover
{

    namespace
    {
        bool supports_access(const RenderResource& resource, ResourceAccess access)
        {
            if (resource.kind == ResourceKind::Buffer)
            {
                return resource_access_supports_buffer(access);
            }
            const bool depth = resource.format == Format::D32_SFLOAT || resource.format == Format::D24_UNORM_S8_UINT ||
                               resource.format == Format::D32_SFLOAT_S8_UINT;
            if (access == ResourceAccess::DepthStencilAttachment)
            {
                return depth;
            }
            if (access == ResourceAccess::ColorAttachment)
            {
                return !depth && resource.format != Format::UNDEFINED;
            }
            return resource_access_supports_texture(access);
        }
    } // namespace

    PassBuilder::PassBuilder(FrameGraph& graph, u32 pass_index) : graph_(graph), pass_index_(pass_index) {}

    void PassBuilder::read(RenderResourceId id, ResourceAccess access)
    {
        graph_.declare_access(pass_index_, id, access, false);
    }

    void PassBuilder::write(RenderResourceId id, ResourceAccess access)
    {
        graph_.declare_access(pass_index_, id, access, true);
    }

    void FrameGraph::invalidate()
    {
        compiled_ = false;
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

    void FrameGraph::declare_access(u32 pass_index, RenderResourceId id, ResourceAccess access, bool write)
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
            return use.resource == id && use.usage == access && use.write == write;
        });
        if (duplicate == uses.end())
        {
            uses.push_back({id, access, write});
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
        res.name              = std::move(name);
        res.kind              = ResourceKind::Texture;
        res.lifetime          = ResourceLifetime::Imported;
        res.texture           = handle;
        res.framebuffer       = fb;
        res.width             = width;
        res.height            = height;
        res.format            = format;
        res.physical_resource = static_cast<RenderResourceId>(resources_.size());
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
        res.name              = std::move(name);
        res.kind              = ResourceKind::Buffer;
        res.lifetime          = ResourceLifetime::Imported;
        res.buffer            = handle;
        res.physical_resource = static_cast<RenderResourceId>(resources_.size());
        resources_.push_back(std::move(res));
        return static_cast<RenderResourceId>(resources_.size() - 1);
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
        resources_.push_back(std::move(version));
        return id;
    }

    const RenderResource& FrameGraph::resource(RenderResourceId id) const
    {
        static const RenderResource invalid{};
        return id < resources_.size() ? resources_[id] : invalid;
    }

    void FrameGraph::set_final_access(RenderResourceId id, ResourceAccess access)
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
        resources_[id].final_access = access;
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

        // Only explicit versions may share an imported physical handle.
        std::vector<RenderResourceId>             successors(resources_.size(), INVALID_RESOURCE_ID);
        std::unordered_map<u64, RenderResourceId> textures;
        std::unordered_map<u64, RenderResourceId> buffers;
        for (RenderResourceId id = 0; id < resources_.size(); ++id)
        {
            const auto& res = resources_[id];
            if (res.previous_version != INVALID_RESOURCE_ID)
            {
                auto& successor = successors[res.previous_version];
                if (successor != INVALID_RESOURCE_ID)
                {
                    ROVER_LOG_ERROR("FrameGraph: resource '{}' has branching versions",
                                    resources_[res.previous_version].name);
                    return false;
                }
                successor = id;
                continue;
            }
            const auto handle = res.kind == ResourceKind::Texture ? res.texture : res.buffer;
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

        std::vector<std::vector<u32>> readers(resources_.size());
        // Gather every producer first, including those registered after readers.
        for (u32 pass_id = 0; pass_id < passes_.size(); ++pass_id)
        {
            const auto&                   pass = passes_[pass_id];
            std::vector<RenderResourceId> versions(resources_.size(), INVALID_RESOURCE_ID);
            auto                          record_version = [&](RenderResourceId id) {
                if (id >= resources_.size())
                {
                    ROVER_LOG_ERROR("FrameGraph: pass '{}' uses invalid resource {}", pass.name, id);
                    return false;
                }
                auto& used = versions[resources_[id].physical_resource];
                if (used != INVALID_RESOURCE_ID && used != id)
                {
                    ROVER_LOG_ERROR("FrameGraph: pass '{}' uses multiple versions of one physical resource", pass.name);
                    return false;
                }
                used = id;
                return true;
            };
            for (auto id : pass.reads)
            {
                if (!record_version(id))
                {
                    return false;
                }
                readers[id].push_back(pass_id);
            }
            for (auto id : pass.writes)
            {
                if (!record_version(id) || !record_write(pass_id, id))
                {
                    return false;
                }
            }
            if (pass.has_clear)
            {
                const auto target = pass.color_attachment;
                if (target >= resources_.size() || resources_[target].kind != ResourceKind::Texture ||
                    !supports_access(resources_[target], ResourceAccess::ColorAttachment) ||
                    resources_[target].framebuffer == INVALID_HANDLE)
                {
                    ROVER_LOG_ERROR("FrameGraph: pass '{}' has an invalid color attachment {}", pass.name, target);
                    return false;
                }
                if (!record_version(target) || !record_write(pass_id, target))
                {
                    return false;
                }
            }
        }

        for (RenderResourceId id = 0; id < resources_.size(); ++id)
        {
            if (resources_[id].previous_version != INVALID_RESOURCE_ID && writers[id] == INVALID_PASS_ID)
            {
                ROVER_LOG_ERROR("FrameGraph: version '{}' has no producer", resources_[id].name);
                return false;
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
        }
        // Preserve every previous-version reader before the physical overwrite,
        // even when the successor's producer was registered first.
        for (RenderResourceId id = 0; id < resources_.size(); ++id)
        {
            const auto previous = resources_[id].previous_version;
            if (previous == INVALID_RESOURCE_ID)
            {
                continue;
            }
            auto& prerequisites = dependencies[writers[id]];
            if (writers[previous] != INVALID_PASS_ID)
            {
                prerequisites.push_back(writers[previous]);
            }
            prerequisites.insert(prerequisites.end(), readers[previous].begin(), readers[previous].end());
        }
        for (u32 pass_id = 0; pass_id < passes_.size(); ++pass_id)
        {
            auto& prerequisites = dependencies[pass_id];
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
        std::vector<std::vector<RenderResourceBarrier>> barriers(passes_.size());
        std::vector<RenderResourceBarrier>              final_barriers;
        std::vector<ResourceAccess>                     states(resources_.size(), ResourceAccess::Undefined);
        for (auto pass_id : order)
        {
            const auto&                   pass = passes_[pass_id];
            std::vector<ResourceAccess>   uses(resources_.size(), ResourceAccess::Undefined);
            std::vector<RenderResourceId> versions(resources_.size(), INVALID_RESOURCE_ID);
            for (const auto& use : pass.accesses)
            {
                // Resource IDs were checked when building the dependency graph.
                auto access = use.usage;
                if (use.write && access == ResourceAccess::Automatic)
                {
                    access = pass.has_clear && use.resource == pass.color_attachment ? ResourceAccess::ColorAttachment
                                                                                     : ResourceAccess::StorageWrite;
                }
                const auto& res        = resources_[use.resource];
                const bool  kind_valid = supports_access(res, access);
                const bool  mode_valid = use.write ? resource_access_is_write(access) : resource_access_is_read(access);
                if (!kind_valid || !mode_valid || access == ResourceAccess::HostRead ||
                    access == ResourceAccess::HostWrite)
                {
                    ROVER_LOG_ERROR("FrameGraph: pass '{}' declares incompatible access {} for resource '{}'",
                                    pass.name,
                                    static_cast<u32>(access),
                                    res.name);
                    return false;
                }
                const auto physical = res.physical_resource;
                if (uses[physical] != ResourceAccess::Undefined && uses[physical] != access)
                {
                    ROVER_LOG_ERROR(
                        "FrameGraph: pass '{}' uses resource '{}' with conflicting usages", pass.name, res.name);
                    return false;
                }
                uses[physical]     = access;
                versions[physical] = use.resource;
            }
            if (pass.has_clear)
            {
                const auto physical = resources_[pass.color_attachment].physical_resource;
                auto&      access   = uses[physical];
                versions[physical]  = pass.color_attachment;
                if (access != ResourceAccess::Undefined && access != ResourceAccess::ColorAttachment)
                {
                    ROVER_LOG_ERROR("FrameGraph: pass '{}' declares a non-color use for its color attachment",
                                    pass.name);
                    return false;
                }
                access = ResourceAccess::ColorAttachment;
            }
            for (RenderResourceId id = 0; id < uses.size(); ++id)
            {
                const auto access = uses[id];
                if (access == ResourceAccess::Undefined)
                {
                    continue;
                }
                if (states[id] != access || resource_access_is_write(states[id]) || resource_access_is_write(access))
                {
                    barriers[pass_id].push_back({versions[id], states[id], access});
                }
                states[id] = access;
            }
        }
        for (RenderResourceId id = 0; id < resources_.size(); ++id)
        {
            const auto& res    = resources_[id];
            const auto  access = res.final_access;
            if (access == ResourceAccess::Undefined)
            {
                continue;
            }
            if (successors[id] != INVALID_RESOURCE_ID)
            {
                ROVER_LOG_ERROR("FrameGraph: only the last version can export resource '{}'", res.name);
                return false;
            }
            const bool valid = supports_access(res, access);
            if (!valid)
            {
                ROVER_LOG_ERROR("FrameGraph: incompatible final access for resource '{}'", res.name);
                return false;
            }
            const auto state = states[res.physical_resource];
            if (state != access || resource_access_is_write(state) || resource_access_is_write(access))
            {
                final_barriers.push_back({id, state, access});
            }
        }
        dependencies_    = std::move(dependencies);
        execution_order_ = std::move(order);
        barriers_        = std::move(barriers);
        final_barriers_  = std::move(final_barriers);
        compiled_        = true;
        return true;
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
                textures.push_back({res.texture, barrier.after});
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
        declaration_error_ = false;
    }

} // namespace rover
