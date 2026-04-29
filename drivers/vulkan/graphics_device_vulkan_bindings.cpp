#include "core/log/log.h"
#include "drivers/vulkan/graphics_device_vulkan.h"

#include <vector>

namespace rover
{

    namespace
    {

        VkDescriptorType to_vk_descriptor_type(BindingType t)
        {
            switch (t)
            {
                case BindingType::UniformBuffer:
                    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                case BindingType::StorageBuffer:
                    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                case BindingType::SampledTexture:
                    return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                case BindingType::StorageTexture:
                    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                case BindingType::Sampler:
                    return VK_DESCRIPTOR_TYPE_SAMPLER;
            }
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }

        VkShaderStageFlags to_vk_shader_stage(ShaderStage s)
        {
            VkShaderStageFlags out = 0;
            if ((s & ShaderStage::Vertex) == ShaderStage::Vertex)
            {
                out |= VK_SHADER_STAGE_VERTEX_BIT;
            }
            if ((s & ShaderStage::Fragment) == ShaderStage::Fragment)
            {
                out |= VK_SHADER_STAGE_FRAGMENT_BIT;
            }
            if ((s & ShaderStage::Compute) == ShaderStage::Compute)
            {
                out |= VK_SHADER_STAGE_COMPUTE_BIT;
            }
            if ((s & ShaderStage::Geometry) == ShaderStage::Geometry)
            {
                out |= VK_SHADER_STAGE_GEOMETRY_BIT;
            }
            if ((s & ShaderStage::TessControl) == ShaderStage::TessControl)
            {
                out |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            }
            if ((s & ShaderStage::TessEval) == ShaderStage::TessEval)
            {
                out |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            }
            return out;
        }

    } // namespace

    // ---------------------------------------------------------------------------
    // Bind group layout
    // ---------------------------------------------------------------------------

    BindGroupLayoutHandle GraphicsDeviceVulkan::create_bind_group_layout(const BindGroupLayoutDesc& desc)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(desc.entries.size());
        for (const auto& e : desc.entries)
        {
            VkDescriptorSetLayoutBinding b{};
            b.binding         = e.binding;
            b.descriptorType  = to_vk_descriptor_type(e.type);
            b.descriptorCount = 1;
            b.stageFlags      = to_vk_shader_stage(e.stage_visibility);
            bindings.push_back(b);
        }

        VkDescriptorSetLayoutCreateInfo info{};
        info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = static_cast<u32>(bindings.size());
        info.pBindings    = bindings.empty() ? nullptr : bindings.data();

        VkBindGroupLayoutResource res{};
        res.entries = desc.entries;
        if (vkCreateDescriptorSetLayout(device_.logical(), &info, nullptr, &res.layout) != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("vkCreateDescriptorSetLayout failed");
            return INVALID_HANDLE;
        }
        return bind_group_layouts_.add(res);
    }

    void GraphicsDeviceVulkan::destroy_bind_group_layout(BindGroupLayoutHandle handle)
    {
        VkBindGroupLayoutResource res{};
        if (!bind_group_layouts_.remove(handle, &res))
        {
            return;
        }
        if (res.layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(device_.logical(), res.layout, nullptr);
        }
    }

    // ---------------------------------------------------------------------------
    // Bind group
    // ---------------------------------------------------------------------------

    BindGroupHandle GraphicsDeviceVulkan::create_bind_group(const BindGroupDesc& desc)
    {
        auto* layout = bind_group_layouts_.get(desc.layout);
        if (layout == nullptr || layout->layout == VK_NULL_HANDLE)
        {
            ROVER_LOG_ERROR("create_bind_group: invalid layout");
            return INVALID_HANDLE;
        }

        VkDescriptorSetAllocateInfo alloc{};
        alloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.descriptorPool     = descriptor_pool_;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts        = &layout->layout;

        VkBindGroupResource res{};
        res.pool   = descriptor_pool_;
        res.layout = desc.layout;
        if (vkAllocateDescriptorSets(device_.logical(), &alloc, &res.set) != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("vkAllocateDescriptorSets failed");
            return INVALID_HANDLE;
        }

        // Build write info for each entry. Buffer/image info structs must outlive
        // the writes vector during the call.
        std::vector<VkWriteDescriptorSet>   writes;
        std::vector<VkDescriptorBufferInfo> buffer_infos;
        std::vector<VkDescriptorImageInfo>  image_infos;
        writes.reserve(desc.entries.size());
        buffer_infos.reserve(desc.entries.size());
        image_infos.reserve(desc.entries.size());

        for (const auto& e : desc.entries)
        {
            VkWriteDescriptorSet w{};
            w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet          = res.set;
            w.dstBinding      = e.binding;
            w.dstArrayElement = 0;
            w.descriptorCount = 1;
            w.descriptorType  = to_vk_descriptor_type(e.type);

            if (e.type == BindingType::UniformBuffer || e.type == BindingType::StorageBuffer)
            {
                auto* buf = buffers_.get(e.buffer);
                if (buf == nullptr)
                {
                    ROVER_LOG_WARN("BindGroup buffer handle invalid (binding {})", e.binding);
                    continue;
                }
                VkDescriptorBufferInfo bi{};
                bi.buffer = buf->buffer;
                bi.offset = e.buffer_offset;
                bi.range  = e.buffer_size != 0 ? e.buffer_size : VK_WHOLE_SIZE;
                buffer_infos.push_back(bi);
                w.pBufferInfo = &buffer_infos.back();
            }
            else
            {
                VkDescriptorImageInfo ii{};
                if (e.type == BindingType::SampledTexture || e.type == BindingType::StorageTexture)
                {
                    auto* tex = textures_.get(e.texture);
                    if (tex == nullptr)
                    {
                        ROVER_LOG_WARN("BindGroup texture handle invalid (binding {})", e.binding);
                        continue;
                    }
                    ii.imageView   = tex->view;
                    ii.imageLayout = e.type == BindingType::SampledTexture ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                                           : VK_IMAGE_LAYOUT_GENERAL;
                }
                if (e.type == BindingType::SampledTexture || e.type == BindingType::Sampler)
                {
                    VkSampler* s = samplers_.get(e.sampler);
                    if (s != nullptr)
                    {
                        ii.sampler = *s;
                    }
                }
                image_infos.push_back(ii);
                w.pImageInfo = &image_infos.back();
            }
            writes.push_back(w);
        }

        if (!writes.empty())
        {
            vkUpdateDescriptorSets(device_.logical(), static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
        }

        return bind_groups_.add(res);
    }

    void GraphicsDeviceVulkan::destroy_bind_group(BindGroupHandle handle)
    {
        VkBindGroupResource res{};
        if (!bind_groups_.remove(handle, &res))
        {
            return;
        }
        if (res.set != VK_NULL_HANDLE && res.pool != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(device_.logical(), res.pool, 1, &res.set);
        }
    }

    // ---------------------------------------------------------------------------
    // Pipeline layout
    // ---------------------------------------------------------------------------

    PipelineLayoutHandle GraphicsDeviceVulkan::create_pipeline_layout(const PipelineLayoutDesc& desc)
    {
        std::vector<VkDescriptorSetLayout> set_layouts;
        set_layouts.reserve(desc.bind_group_layouts.size());
        for (const auto h : desc.bind_group_layouts)
        {
            auto* l = bind_group_layouts_.get(h);
            if (l == nullptr || l->layout == VK_NULL_HANDLE)
            {
                ROVER_LOG_ERROR("create_pipeline_layout: invalid bind group layout");
                return INVALID_HANDLE;
            }
            set_layouts.push_back(l->layout);
        }

        std::vector<VkPushConstantRange> push_ranges;
        push_ranges.reserve(desc.push_constants.size());
        for (const auto& pc : desc.push_constants)
        {
            VkPushConstantRange r{};
            r.stageFlags = to_vk_shader_stage(pc.stage_visibility);
            r.offset     = pc.offset;
            r.size       = pc.size;
            push_ranges.push_back(r);
        }

        VkPipelineLayoutCreateInfo info{};
        info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.setLayoutCount         = static_cast<u32>(set_layouts.size());
        info.pSetLayouts            = set_layouts.empty() ? nullptr : set_layouts.data();
        info.pushConstantRangeCount = static_cast<u32>(push_ranges.size());
        info.pPushConstantRanges    = push_ranges.empty() ? nullptr : push_ranges.data();

        VkPipelineLayoutResource res{};
        res.bind_group_layouts = desc.bind_group_layouts;
        res.push_constants     = desc.push_constants;
        if (vkCreatePipelineLayout(device_.logical(), &info, nullptr, &res.layout) != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("vkCreatePipelineLayout failed");
            return INVALID_HANDLE;
        }
        return pipeline_layouts_.add(res);
    }

    void GraphicsDeviceVulkan::destroy_pipeline_layout(PipelineLayoutHandle handle)
    {
        VkPipelineLayoutResource res{};
        if (!pipeline_layouts_.remove(handle, &res))
        {
            return;
        }
        if (res.layout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device_.logical(), res.layout, nullptr);
        }
    }

    // ---------------------------------------------------------------------------
    // Command-side bindings
    // ---------------------------------------------------------------------------

    void GraphicsDeviceVulkan::cmd_bind_group(CommandListHandle cmd, u32 set_index, BindGroupHandle group)
    {
        auto* cl = command_lists_.get(cmd);
        auto* bg = bind_groups_.get(group);
        if (cl == nullptr || bg == nullptr)
        {
            ROVER_LOG_WARN("cmd_bind_group: invalid handle");
            return;
        }
        if (cl->current_layout == VK_NULL_HANDLE)
        {
            ROVER_LOG_WARN("cmd_bind_group: no pipeline bound (call cmd_bind_pipeline first)");
            return;
        }
        vkCmdBindDescriptorSets(
            cl->buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cl->current_layout, set_index, 1, &bg->set, 0, nullptr);
    }

    void GraphicsDeviceVulkan::cmd_push_constants(CommandListHandle    cmd,
                                                  PipelineLayoutHandle layout,
                                                  ShaderStage          stage,
                                                  const void*          data,
                                                  u32                  size,
                                                  u32                  offset)
    {
        auto* cl = command_lists_.get(cmd);
        auto* pl = pipeline_layouts_.get(layout);
        if (cl == nullptr || pl == nullptr || pl->layout == VK_NULL_HANDLE)
        {
            ROVER_LOG_WARN("cmd_push_constants: invalid handle");
            return;
        }
        vkCmdPushConstants(cl->buffer, pl->layout, to_vk_shader_stage(stage), offset, size, data);
    }

} // namespace rover
