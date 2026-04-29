# ADR-0010: GraphicsDevice Descriptor Binding 设计

- **Status**: Accepted
- **Date**: 2026-04-29
- **Deciders**: core team

## Context

Phase 2 需要把 shader 与资源（uniform buffer / storage buffer / texture / sampler）绑定到管线，让 mesh + camera + light demo 跑得起来。Phase 1 的 `GraphicsDevice` 抽象只暴露了 `cmd_bind_pipeline` 和 `cmd_bind_vertex_buffer`，没有任何 descriptor 体系。补全设计时面临两条路线：

1. **Vulkan-原生风格**（DescriptorSetLayout / DescriptorSet）：直接把 Vulkan 概念抬到核心抽象层，driver 实现近乎一对一映射。
2. **WebGPU 风格**（BindGroupLayout / BindGroup）：跨后端中性命名，行为对齐 WebGPU spec，方便未来 D3D12（root signature）与 Metal（argument buffer）的实现按概念翻译。

两条路线在表达力上等价（都能描述「集合-绑定-类型」三元组），区别在命名和未来扩展性。

## Decision

采用 **WebGPU 风格的 BindGroup / BindGroupLayout / PipelineLayout** 命名与结构，并扩展 `GraphicsDevice` 接口：

新增公共类型：

- `BindingType`（UniformBuffer / StorageBuffer / SampledTexture / StorageTexture / Sampler）
- `BindGroupLayoutEntry { binding, type, stage_visibility }`
- `BindGroupLayoutDesc / BindGroupLayoutHandle`
- `BindGroupEntry { binding, type, buffer/texture/sampler 三选一字段 }`
- `BindGroupDesc / BindGroupHandle`
- `PushConstantRange { stage_visibility, offset, size }`
- `PipelineLayoutDesc { bind_group_layouts, push_constants }`
- `PipelineLayoutHandle`

新增 `GraphicsDevice` 虚方法：

- `create_bind_group_layout / destroy_bind_group_layout`
- `create_bind_group / destroy_bind_group`
- `create_pipeline_layout / destroy_pipeline_layout`
- `cmd_bind_group(cmd, set_index, group)`
- `cmd_push_constants(cmd, layout, stage, data, size, offset)`

`GraphicsPipelineDesc` 增加可选字段 `pipeline_layout`：当为 `INVALID_HANDLE` 时驱动创建空 layout（Phase 1 三角形 demo 路径），否则使用调用方传入的 layout。

Vulkan driver 实现上：

- `BindingType` 直接映射到 `VkDescriptorType`；
- `cmd_bind_pipeline` 顺手把当前 pipeline 的 `VkPipelineLayout` 缓存到 `VkCommandListResource::current_layout`，让 `cmd_bind_group` / `cmd_push_constants` 不必再回到核心抽象拿 layout；
- 描述符池采用单一全局池（1024 set）（Phase 2 demo 足够，未来材质系统再分桶）。

## Consequences

**Positive:**

- 命名跨后端中性，未来加 D3D12 / Metal / WebGPU 后端时不必重命名（只是翻译实现）；
- WebGPU 风格的「set 数量受限 + 强类型 entry」迫使我们提早思考 binding 布局，比 Vulkan 任意 binding number 更利于跨后端共存；
- Push constant 单独建模，比塞进 BindGroup 更接近硬件实际行为；
- 透过 `VkCommandListResource::current_layout` 缓存，调用方不需要在调用 `cmd_bind_group` 时再传一次 PipelineLayoutHandle（与 WebGPU 隐式绑定语义一致）。

**Negative / Trade-offs:**

- BindingType 暂不支持 dynamic uniform / dynamic storage（Vulkan 区分这两种），未来如需要再扩 enum；
- Vulkan 描述符池单池策略在大型项目里会碰到 fragmentation；
- BindGroup 中的资源 handle 在 driver 端写入时必须是「已就绪」状态（buffer 已上传 / texture 已 transition 到 SHADER_READ_ONLY_OPTIMAL），目前由 caller 保证；后续 Frame Graph 会自动管理这件事。

## Alternatives Considered

- **Vulkan-原生**：实现成本最低，但未来加 D3D12 时 root signature 概念差异大，会产生「术语在 Vulkan 是 set，在 D3D12 是 root_param」的双语义代码；
- **不抽象，让上层每个后端写一份**：违反 ADR-0001 三层倒置；
- **Bindless 优先**：Phase 2 太早，Bindless 不在所有目标平台（Web / 老移动 GPU）可用，留到 Phase 4+ 再评估。

## References

- 接口：[core/graphics/graphics_device.h](../../../core/graphics/graphics_device.h), [core/graphics/graphics_desc.h](../../../core/graphics/graphics_desc.h)
- Vulkan 实现：[drivers/vulkan/graphics_device_vulkan_bindings.cpp](../../../drivers/vulkan/graphics_device_vulkan_bindings.cpp)
- WebGPU 规范：<https://www.w3.org/TR/webgpu/#bind-group-layout>
- 关联需求：[F-008](../REQUIREMENTS.md)
- 关联 ADR：[ADR-0001](ADR-0001-three-layer-architecture.md)
