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

    namespace
    {
        bool supports_access(const RenderResource& resource, ResourceAccess access)
        {
            if (resource.lifetime == ResourceLifetime::Transient)
            {
                if (resource.kind == ResourceKind::Buffer)
                {
                    const auto& desc = resource.buffer_desc;
                    auto        has  = [&](BufferUsage flag) {
                        return (desc.usage & flag) == flag;
                    };
                    switch (access)
                    {
                        case ResourceAccess::ShaderRead:
                        case ResourceAccess::StorageRead:
                        case ResourceAccess::StorageWrite:
                            return has(BufferUsage::Storage);
                        case ResourceAccess::UniformRead:
                            return has(BufferUsage::Uniform);
                        case ResourceAccess::VertexRead:
                            return has(BufferUsage::Vertex);
                        case ResourceAccess::IndexRead:
                            return has(BufferUsage::Index);
                        case ResourceAccess::IndirectRead:
                            return has(BufferUsage::Indirect);
                        case ResourceAccess::HostRead:
                        case ResourceAccess::HostWrite:
                            return desc.memory != MemoryUsage::GpuOnly;
                        default:
                            return resource_access_supports_buffer(access);
                    }
                }
                auto has = [&](TextureUsage flag) {
                    return (resource.texture_desc.usage & flag) == flag;
                };
                switch (access)
                {
                    case ResourceAccess::ShaderRead:
                        if (!has(TextureUsage::Sampled))
                        {
                            return false;
                        }
                        break;
                    case ResourceAccess::StorageRead:
                    case ResourceAccess::StorageWrite:
                        if (!has(TextureUsage::Storage))
                        {
                            return false;
                        }
                        break;
                    case ResourceAccess::ColorAttachment:
                        if (!has(TextureUsage::ColorAttachment))
                        {
                            return false;
                        }
                        break;
                    case ResourceAccess::DepthStencilAttachment:
                        if (!has(TextureUsage::DepthStencilAttachment))
                        {
                            return false;
                        }
                        break;
                    case ResourceAccess::TransferRead:
                        if (!has(TextureUsage::TransferSrc))
                        {
                            return false;
                        }
                        break;
                    case ResourceAccess::TransferWrite:
                        break; // Drivers add transfer-destination usage.
                    default:
                        return false;
                }
            }
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
        pass.render_pass         = INVALID_HANDLE;
        pass.depth_attachment    = INVALID_RESOURCE_ID;
        pass.has_clear           = true;
        pass.clear_depth_stencil = clear_depth_stencil;
        pass.clear               = clear;
    }

    void FrameGraph::set_render_target(u32               pass_index,
                                       RenderPassHandle  render_pass,
                                       RenderResourceId  color,
                                       const ClearValue& clear,
                                       RenderResourceId  depth)
    {
        set_color_attachment(pass_index, color, clear, depth != INVALID_RESOURCE_ID);
        if (!executing_ && pass_index < passes_.size())
        {
            passes_[pass_index].render_pass      = render_pass;
            passes_[pass_index].depth_attachment = depth;
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
            if (res.lifetime == ResourceLifetime::Transient)
            {
                const auto& texture = res.texture_desc;
                if ((res.kind == ResourceKind::Texture &&
                     (texture.width == 0 || texture.height == 0 || texture.depth == 0 || texture.mip_levels == 0 ||
                      texture.array_layers == 0 || texture.format == Format::UNDEFINED ||
                      static_cast<u32>(texture.usage) == 0)) ||
                    (res.kind == ResourceKind::Buffer && res.buffer_desc.size == 0))
                {
                    ROVER_LOG_ERROR("FrameGraph: invalid transient descriptor '{}'", res.name);
                    return false;
                }
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
                    (resources_[target].framebuffer == INVALID_HANDLE && pass.render_pass == INVALID_HANDLE))
                {
                    ROVER_LOG_ERROR("FrameGraph: pass '{}' has an invalid color attachment {}", pass.name, target);
                    return false;
                }
                if (!record_version(target) || !record_write(pass_id, target))
                {
                    return false;
                }
                if (pass.depth_attachment != INVALID_RESOURCE_ID)
                {
                    const auto depth = pass.depth_attachment;
                    if (!record_version(depth) || resources_[depth].kind != ResourceKind::Texture ||
                        !supports_access(resources_[depth], ResourceAccess::DepthStencilAttachment) ||
                        resources_[depth].width != resources_[target].width ||
                        resources_[depth].height != resources_[target].height || !record_write(pass_id, depth))
                    {
                        ROVER_LOG_ERROR("FrameGraph: invalid depth attachment in '{}'", pass.name);
                        return false;
                    }
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

        for (RenderResourceId id = 0; id < resources_.size(); ++id)
        {
            const auto& resource = resources_[id];
            if (resource.physical_resource == id && resource.lifetime == ResourceLifetime::Transient &&
                writers[id] == INVALID_PASS_ID &&
                (!readers[id].empty() || resource_access_is_read(resource.final_access)) &&
                !(resource.kind == ResourceKind::Buffer && resource.buffer_desc.memory == MemoryUsage::CpuToGpu))
            {
                ROVER_LOG_ERROR("FrameGraph: transient '{}' is read without a producer", resource.name);
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
                if (pass.depth_attachment != INVALID_RESOURCE_ID)
                {
                    const auto depth_physical = resources_[pass.depth_attachment].physical_resource;
                    if (uses[depth_physical] != ResourceAccess::Undefined &&
                        uses[depth_physical] != ResourceAccess::DepthStencilAttachment)
                    {
                        ROVER_LOG_ERROR("FrameGraph: conflicting depth attachment access in '{}'", pass.name);
                        return false;
                    }
                    uses[depth_physical]     = ResourceAccess::DepthStencilAttachment;
                    versions[depth_physical] = pass.depth_attachment;
                }
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
        for (auto& resource : resources_)
        {
            resource.first_use = resource.last_use = INVALID_PASS_ID;
        }
        for (u32 position = 0; position < order.size(); ++position)
        {
            const auto& pass  = passes_[order[position]];
            auto        touch = [&](RenderResourceId id) {
                auto& root     = resources_[resources_[id].physical_resource];
                root.first_use = std::min(root.first_use, position);
                root.last_use  = position;
            };
            for (const auto& access : pass.accesses)
            {
                touch(access.resource);
            }
            if (pass.has_clear)
            {
                touch(pass.color_attachment);
            }
            if (pass.depth_attachment != INVALID_RESOURCE_ID)
            {
                touch(pass.depth_attachment);
            }
        }
        for (const auto& resource : resources_)
        {
            if (resource.final_access != ResourceAccess::Undefined)
            {
                {
                    auto& root = resources_[resource.physical_resource];
                    if (root.first_use == INVALID_PASS_ID)
                    {
                        root.first_use = static_cast<u32>(order.size());
                    }
                    root.last_use = static_cast<u32>(order.size());
                }
            }
        }
        for (auto& resource : resources_)
        {
            const auto& root   = resources_[resource.physical_resource];
            resource.first_use = root.first_use;
            resource.last_use  = root.last_use;
        }
        for (const auto& group : bind_groups_)
        {
            std::vector<u32> slots;
            if (!group.layout)
            {
                ROVER_LOG_ERROR("FrameGraph: invalid bind group layout");
                return false;
            }
            for (const auto& binding : group.entries)
            {
                const auto& entry = binding.entry;
                if (std::find(slots.begin(), slots.end(), entry.binding) != slots.end())
                {
                    ROVER_LOG_ERROR("FrameGraph: duplicate binding slot");
                    return false;
                }
                slots.push_back(entry.binding);
                if (entry.type == BindingType::Sampler)
                {
                    if (binding.resource == INVALID_RESOURCE_ID && entry.sampler)
                    {
                        continue;
                    }
                }
                else if (binding.resource < resources_.size())
                {
                    const auto& resource = resources_[binding.resource];
                    const bool  buffer =
                        entry.type == BindingType::UniformBuffer || entry.type == BindingType::StorageBuffer;
                    if (resource.first_use != INVALID_PASS_ID && buffer == (resource.kind == ResourceKind::Buffer))
                    {
                        if (!buffer || resource.lifetime == ResourceLifetime::Imported ||
                            (entry.buffer_offset < resource.buffer_desc.size &&
                             (entry.buffer_size == 0 ||
                              entry.buffer_size <= resource.buffer_desc.size - entry.buffer_offset)))
                        {
                            continue;
                        }
                    }
                }
                ROVER_LOG_ERROR("FrameGraph: invalid or unused bind group resource");
                return false;
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
        bind_groups_.clear();
        declaration_error_ = false;
    }

} // namespace rover
