#include "core/log/log.h"
#include "services/graphics/frame_graph.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_map>

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

        struct CellUse
        {
            RenderResourceId        resource;
            usize                   cell;
            usize                   physical;
            ResourceAccess          access;
            bool                    write;
            TextureSubresourceRange range;
        };

        struct VersionCells
        {
            std::vector<u32>              writers;
            std::vector<std::vector<u32>> readers;
            std::vector<u32>              producers;
            std::vector<std::vector<u32>> live_readers;
        };

        u32 layers(const RenderResource& resource)
        {
            return resource.texture_desc.array_layers *
                   (resource.texture_desc.type == TextureType::TextureCube ? 6u : 1u);
        }

        bool normalize(const RenderResource& resource, TextureSubresourceRange input, TextureSubresourceRange& output)
        {
            if (resource.kind == ResourceKind::Buffer)
            {
                output = {};
                return input == TextureSubresourceRange{};
            }
            return normalize_texture_range(
                resource.format, resource.texture_desc.mip_levels, layers(resource), input, output);
        }

        // Preserve a small barrier plan, including combined depth/stencil ranges
        // on devices where separateDepthStencilLayouts is unavailable.
        void coalesce(std::vector<RenderResourceBarrier>& barriers)
        {
            bool changed = true;
            while (changed)
            {
                changed = false;
                for (usize i = 0; i < barriers.size() && !changed; ++i)
                {
                    for (usize j = i + 1; j < barriers.size(); ++j)
                    {
                        auto&       a = barriers[i];
                        const auto& b = barriers[j];
                        if (a.resource != b.resource || a.before != b.before || a.after != b.after)
                        {
                            continue;
                        }
                        auto&       x     = a.range;
                        const auto& y     = b.range;
                        bool        merge = false;
                        if (x.base_mip == y.base_mip && x.mip_count == y.mip_count && x.base_layer == y.base_layer &&
                            x.layer_count == y.layer_count && (x.aspects & y.aspects) == TextureAspect::Automatic)
                        {
                            x.aspects = x.aspects | y.aspects;
                            merge     = true;
                        }
                        else if (x.aspects == y.aspects && x.base_mip == y.base_mip && x.mip_count == y.mip_count &&
                                 x.base_layer + x.layer_count == y.base_layer)
                        {
                            x.layer_count += y.layer_count;
                            merge = true;
                        }
                        else if (x.aspects == y.aspects && x.base_layer == y.base_layer &&
                                 x.layer_count == y.layer_count && x.base_mip + x.mip_count == y.base_mip)
                        {
                            x.mip_count += y.mip_count;
                            merge = true;
                        }
                        if (merge)
                        {
                            barriers.erase(barriers.begin() + static_cast<std::ptrdiff_t>(j));
                            changed = true;
                            break;
                        }
                    }
                }
            }
        }
    } // namespace

    bool FrameGraph::compile()
    {
        if (executing_)
        {
            return false;
        }
        invalidate();
        auto fail = [](const std::string& reason) {
            ROVER_LOG_ERROR("FrameGraph: {}", reason);
            return false;
        };
        if (declaration_error_)
        {
            return fail("invalid declarations; reset before rebuilding");
        }
        const auto                                size = resources_.size();
        std::vector<RenderResourceId>             successors(size, INVALID_RESOURCE_ID);
        std::vector<usize>                        offsets(size), counts(size);
        std::unordered_map<u64, RenderResourceId> textures, buffers;
        usize                                     total_cells = 0;
        for (RenderResourceId id = 0; id < size; ++id)
        {
            const auto& resource = resources_[id];
            if (resource.previous_version != INVALID_RESOURCE_ID)
            {
                auto& next = successors[resource.previous_version];
                if (next != INVALID_RESOURCE_ID)
                {
                    return fail("branching resource versions: " + resource.name);
                }
                next        = id;
                offsets[id] = offsets[resource.physical_resource];
                counts[id]  = counts[resource.physical_resource];
                continue;
            }
            if (resource.kind == ResourceKind::Texture)
            {
                const auto& desc = resource.texture_desc;
                if (desc.mip_levels == 0 || desc.mip_levels > 32 || desc.array_layers == 0 ||
                    (desc.type == TextureType::TextureCube && desc.array_layers > std::numeric_limits<u32>::max() / 6))
                {
                    return fail("invalid texture dimensions: " + resource.name);
                }
                counts[id] = static_cast<usize>(3) * desc.mip_levels * layers(resource);
            }
            else
            {
                counts[id] = 1;
            }
            if (counts[id] > std::numeric_limits<usize>::max() - total_cells)
            {
                return fail("subresource count overflow");
            }
            offsets[id] = total_cells;
            total_cells += counts[id];
            if (resource.lifetime == ResourceLifetime::Transient)
            {
                const auto& desc = resource.texture_desc;
                if ((resource.kind == ResourceKind::Texture &&
                     (!desc.width || !desc.height || !desc.depth || resource.format == Format::UNDEFINED ||
                      desc.usage == TextureUsage::None)) ||
                    (resource.kind == ResourceKind::Buffer && !resource.buffer_desc.size))
                {
                    return fail("invalid transient descriptor: " + resource.name);
                }
            }
            else
            {
                const auto handle = resource.kind == ResourceKind::Texture ? resource.texture : resource.buffer;
                if (!handle)
                {
                    return fail("invalid imported handle: " + resource.name);
                }
                auto& imports = resource.kind == ResourceKind::Texture ? textures : buffers;
                if (!imports.emplace(handle, id).second)
                {
                    return fail("duplicate physical import: " + resource.name);
                }
            }
        }

        std::vector<VersionCells> versions(size);
        for (RenderResourceId id = 0; id < size; ++id)
        {
            versions[id].writers.assign(counts[id], INVALID_PASS_ID);
            versions[id].readers.resize(counts[id]);
            versions[id].producers.assign(counts[id], INVALID_PASS_ID);
            versions[id].live_readers.resize(counts[id]);
        }
        std::vector<std::vector<CellUse>> pass_uses(passes_.size());
        for (u32 pass_id = 0; pass_id < passes_.size(); ++pass_id)
        {
            const auto& pass = passes_[pass_id];
            auto&       uses = pass_uses[pass_id];
            auto add = [&](RenderResourceId id, ResourceAccess access, bool write, TextureSubresourceRange input) {
                if (id >= size)
                {
                    return false;
                }
                const auto&             resource = resources_[id];
                TextureSubresourceRange range{};
                if (!normalize(resource, input, range))
                {
                    return false;
                }
                if (write && access == ResourceAccess::Automatic)
                {
                    TextureSubresourceRange color{};
                    access = pass.has_clear && id == pass.color_attachment &&
                                     normalize(resource, pass.color_range, color) && color == range
                                 ? ResourceAccess::ColorAttachment
                                 : ResourceAccess::StorageWrite;
                }
                if (!supports_access(resource, access) ||
                    !(write ? resource_access_is_write(access) : resource_access_is_read(access)) ||
                    access == ResourceAccess::HostRead || access == ResourceAccess::HostWrite)
                {
                    return false;
                }
                bool valid = true;
                auto cell  = [&](usize local, TextureSubresourceRange single) {
                    const auto physical = offsets[id] + local;
                    for (const auto& other : uses)
                    {
                        if (other.physical != physical)
                        {
                            continue;
                        }
                        if (other.resource != id || other.access != access || other.write != write)
                        {
                            valid = false;
                        }
                        return;
                    }
                    uses.push_back({id, local, physical, access, write, single});
                    if (write)
                    {
                        auto& writer = versions[id].writers[local];
                        if (writer != INVALID_PASS_ID && writer != pass_id)
                        {
                            valid = false;
                        }
                        writer = pass_id;
                    }
                    else
                    {
                        versions[id].readers[local].push_back(pass_id);
                    }
                };
                if (resource.kind == ResourceKind::Buffer)
                {
                    cell(0, {});
                }
                else
                {
                    for_texture_cells(range, [&](auto aspect, auto mip, auto layer) {
                        cell(texture_cell_index(aspect, mip, layer, resource.texture_desc.mip_levels, layers(resource)),
                             {aspect, mip, 1, layer, 1});
                    });
                }
                return valid;
            };
            for (const auto& access : pass.accesses)
            {
                if (!add(access.resource, access.usage, access.write, access.range))
                {
                    return fail("invalid, overlapping or conflicting access in pass '" + pass.name + "'");
                }
            }
            if (pass.has_clear)
            {
                const auto              color = pass.color_attachment;
                TextureSubresourceRange range{};
                if (color >= size || resources_[color].kind != ResourceKind::Texture ||
                    !normalize(resources_[color], pass.color_range, range) || range.mip_count != 1 ||
                    range.layer_count != 1 ||
                    (resources_[color].framebuffer == INVALID_HANDLE && pass.render_pass == INVALID_HANDLE) ||
                    !add(color, ResourceAccess::ColorAttachment, true, range))
                {
                    return fail("invalid color attachment: " + pass.name);
                }
                if (pass.depth_attachment != INVALID_RESOURCE_ID)
                {
                    const auto              depth = pass.depth_attachment;
                    TextureSubresourceRange dr{};
                    if (depth >= size || resources_[depth].kind != ResourceKind::Texture ||
                        !normalize(resources_[depth], pass.depth_range, dr) || dr.mip_count != 1 ||
                        dr.layer_count != 1 || dr.aspects != texture_aspects(resources_[depth].format) ||
                        texture_mip_extent(resources_[depth].width, dr.base_mip) !=
                            texture_mip_extent(resources_[color].width, range.base_mip) ||
                        texture_mip_extent(resources_[depth].height, dr.base_mip) !=
                            texture_mip_extent(resources_[color].height, range.base_mip) ||
                        !add(depth, ResourceAccess::DepthStencilAttachment, true, dr))
                    {
                        return fail("invalid depth attachment: " + pass.name);
                    }
                }
            }
        }
        std::vector<std::vector<u32>> dependencies(passes_.size());
        for (RenderResourceId id = 0; id < size; ++id)
        {
            const auto& resource = resources_[id];
            auto&       current  = versions[id];
            if (resource.previous_version != INVALID_RESOURCE_ID &&
                std::none_of(current.writers.begin(), current.writers.end(), [](auto writer) {
                    return writer != INVALID_PASS_ID;
                }))
            {
                return fail("version has no producer: " + resource.name);
            }
            for (usize cell = 0; cell < counts[id]; ++cell)
            {
                const auto writer = current.writers[cell];
                if (resource.previous_version != INVALID_RESOURCE_ID)
                {
                    const auto& previous = versions[resource.previous_version];
                    if (writer != INVALID_PASS_ID)
                    {
                        auto& before = dependencies[writer];
                        if (previous.producers[cell] != INVALID_PASS_ID)
                        {
                            before.push_back(previous.producers[cell]);
                        }
                        before.insert(
                            before.end(), previous.live_readers[cell].begin(), previous.live_readers[cell].end());
                    }
                    else
                    {
                        current.producers[cell]    = previous.producers[cell];
                        current.live_readers[cell] = previous.live_readers[cell];
                    }
                }
                if (writer != INVALID_PASS_ID)
                {
                    current.producers[cell] = writer;
                }
                const auto producer = current.producers[cell];
                for (auto reader : current.readers[cell])
                {
                    if (producer == reader)
                    {
                        return fail("same-pass feedback: " + resource.name);
                    }
                    if (producer != INVALID_PASS_ID)
                    {
                        dependencies[reader].push_back(producer);
                    }
                    else if (resource.lifetime == ResourceLifetime::Transient &&
                             !(resource.kind == ResourceKind::Buffer &&
                               resource.buffer_desc.memory == MemoryUsage::CpuToGpu))
                    {
                        return fail("transient subresource read without initialization: " + resource.name);
                    }
                    current.live_readers[cell].push_back(reader);
                }
            }
        }
        std::vector<std::vector<u32>>                                 consumers(passes_.size());
        std::vector<usize>                                            indegree(passes_.size());
        std::priority_queue<u32, std::vector<u32>, std::greater<u32>> ready;
        for (u32 p = 0; p < passes_.size(); ++p)
        {
            auto& before = dependencies[p];
            std::sort(before.begin(), before.end());
            before.erase(std::unique(before.begin(), before.end()), before.end());
            indegree[p] = before.size();
            for (auto dependency : before)
            {
                consumers[dependency].push_back(p);
            }
            if (!indegree[p])
            {
                ready.push(p);
            }
        }
        std::vector<u32> order;
        while (!ready.empty())
        {
            const auto pass = ready.top();
            ready.pop();
            order.push_back(pass);
            for (auto next : consumers[pass])
            {
                if (--indegree[next] == 0)
                {
                    ready.push(next);
                }
            }
        }
        if (order.size() != passes_.size())
        {
            return fail("cyclic data or overwrite dependencies");
        }
        std::vector<ResourceAccess>                     states(total_cells, ResourceAccess::Undefined);
        std::vector<std::vector<RenderResourceBarrier>> barriers(passes_.size());
        for (auto pass : order)
        {
            for (const auto& use : pass_uses[pass])
            {
                auto& state = states[use.physical];
                if (state != use.access || resource_access_is_write(state) || use.write)
                {
                    barriers[pass].push_back({use.resource, state, use.access, use.range});
                }
                state = use.access;
            }
            coalesce(barriers[pass]);
        }
        std::vector<RenderResourceBarrier> final_barriers;
        std::vector<ResourceAccess>        exported(total_cells, ResourceAccess::Undefined);
        for (RenderResourceId id = 0; id < size; ++id)
        {
            auto exports = resources_[id].exports;
            if (resources_[id].final_access != ResourceAccess::Undefined)
            {
                exports.push_back({resources_[id].final_access, {}});
            }
            auto last = id;
            while (successors[last] != INVALID_RESOURCE_ID)
            {
                last = successors[last];
            }
            for (const auto& request : exports)
            {
                TextureSubresourceRange range{};
                if (!normalize(resources_[id], request.range, range) ||
                    !supports_access(resources_[id], request.access))
                {
                    return fail("invalid export range or access: " + resources_[id].name);
                }
                bool valid       = true;
                auto export_cell = [&](usize cell, TextureSubresourceRange single) {
                    const auto physical = offsets[id] + cell;
                    const auto producer = versions[id].producers[cell];
                    if (producer != versions[last].producers[cell] ||
                        (exported[physical] != ResourceAccess::Undefined && exported[physical] != request.access))
                    {
                        valid = false;
                    }
                    if (resource_access_is_read(request.access) && producer == INVALID_PASS_ID &&
                        resources_[id].lifetime == ResourceLifetime::Transient &&
                        !(resources_[id].kind == ResourceKind::Buffer &&
                          resources_[id].buffer_desc.memory == MemoryUsage::CpuToGpu))
                    {
                        valid = false;
                    }
                    if (exported[physical] != ResourceAccess::Undefined)
                    {
                        return;
                    }
                    exported[physical] = request.access;
                    const auto before  = states[physical];
                    if (before != request.access || resource_access_is_write(before) ||
                        resource_access_is_write(request.access))
                    {
                        final_barriers.push_back({id, before, request.access, single});
                    }
                };
                if (resources_[id].kind == ResourceKind::Buffer)
                {
                    export_cell(0, {});
                }
                else
                {
                    for_texture_cells(range, [&](auto aspect, auto mip, auto layer) {
                        export_cell(
                            texture_cell_index(
                                aspect, mip, layer, resources_[id].texture_desc.mip_levels, layers(resources_[id])),
                            {aspect, mip, 1, layer, 1});
                    });
                }
                if (!valid)
                {
                    return fail("stale version, conflicting export or uninitialized subresource: " +
                                resources_[id].name);
                }
            }
        }
        coalesce(final_barriers);
        auto touch = [&](RenderResourceId id, u32 position) {
            auto& root     = resources_[resources_[id].physical_resource];
            root.first_use = std::min(root.first_use, position);
            if (root.last_use == INVALID_PASS_ID)
            {
                root.last_use = position;
            }
            else
            {
                root.last_use = std::max(root.last_use, position);
            }
        };
        for (u32 position = 0; position < order.size(); ++position)
        {
            for (const auto& use : pass_uses[order[position]])
            {
                touch(use.resource, position);
            }
        }
        for (RenderResourceId id = 0; id < size; ++id)
        {
            if (resources_[id].final_access != ResourceAccess::Undefined || !resources_[id].exports.empty())
            {
                touch(id, static_cast<u32>(order.size()));
            }
        }
        for (auto& resource : resources_)
        {
            resource.first_use = resources_[resource.physical_resource].first_use;
            resource.last_use  = resources_[resource.physical_resource].last_use;
        }
        for (const auto& group : bind_groups_)
        {
            if (!group.layout)
            {
                return fail("invalid bind group layout");
            }
            std::vector<u32> slots;
            for (const auto& binding : group.entries)
            {
                const auto& entry = binding.entry;
                if (std::find(slots.begin(), slots.end(), entry.binding) != slots.end())
                {
                    return fail("duplicate binding slot");
                }
                slots.push_back(entry.binding);
                if (entry.type == BindingType::Sampler)
                {
                    if (binding.resource == INVALID_RESOURCE_ID && entry.sampler)
                    {
                        continue;
                    }
                }
                else if (binding.resource < size && resources_[binding.resource].first_use != INVALID_PASS_ID)
                {
                    const auto& resource = resources_[binding.resource];
                    const bool  buffer =
                        entry.type == BindingType::UniformBuffer || entry.type == BindingType::StorageBuffer;
                    if (buffer && resource.kind == ResourceKind::Buffer)
                    {
                        if (resource.lifetime == ResourceLifetime::Imported ||
                            (entry.buffer_offset < resource.buffer_desc.size &&
                             (!entry.buffer_size ||
                              entry.buffer_size <= resource.buffer_desc.size - entry.buffer_offset)))
                        {
                            continue;
                        }
                    }
                    else if (!buffer && resource.kind == ResourceKind::Texture)
                    {
                        TextureSubresourceRange range{};
                        if (normalize(resource, entry.texture_range, range) &&
                            (range.aspects == TextureAspect::Color || range.aspects == TextureAspect::Depth ||
                             range.aspects == TextureAspect::Stencil))
                        {
                            continue;
                        }
                    }
                }
                return fail("invalid or unused bind group resource");
            }
        }
        dependencies_    = std::move(dependencies);
        execution_order_ = std::move(order);
        barriers_        = std::move(barriers);
        final_barriers_  = std::move(final_barriers);
        compiled_        = true;
        return true;
    }
} // namespace rover
