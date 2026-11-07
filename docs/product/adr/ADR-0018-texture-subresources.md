# ADR-0018: 纹理子资源访问、视图与同步

- **Status**: Accepted
- **Date**: 2026-11-07
- **Deciders**: core team

## Context

Phase 2 的资源池与版本链已经完成。纹理仍用单一布局与访问状态，不能表达不同 mip、array layer 或 depth/stencil aspect 的独立访问；上传、采样视图、附件和回读也需要匹配相同范围。

## Decision

core 定义 TextureAspect、TextureSubresourceRange 和 TextureCopyRegion。范围默认覆盖格式的全部 aspect、剩余 mip 与 layer；复制默认选择 mip 0、layer 0；组合 depth/stencil 格式须显式选择单个 aspect。集中验证、规范化与子资源遍历，保持已有调用的默认行为。

FrameGraph 的 read/write、导出和 render target 支持范围，导入纹理可提供完整 TextureDesc。编译按 aspect/mip/layer 单元确定生产者、读者和访问状态。同一版本的互不重叠范围可以由不同 pass 写入；重叠单元保持单生产者。版本的未写入单元继承前版本内容，RAW/WAR/WAW 仅约束重叠单元。同 pass 可以访问不重叠范围，重叠反馈仍拒绝。导出验证每个单元的最后内容版本，物理对象的租约仍覆盖全部单元的使用区间。

Vulkan 为每个子资源维护最近访问、最后写入及后续读范围。整个 barrier 批次先验证，重叠请求拒绝且不修改状态；成功后记录对应范围。驱动按规范查询并启用 separateDepthStencilLayouts；不支持时保留组合 depth/stencil 的同步，部分 aspect 操作明确失败。

FramebufferDesc 和 BindGroupEntry 携带纹理范围。驱动在纹理内缓存匹配的 VkImageView，framebuffer 使用单 mip/layer，采样视图包含声明的范围。视图随纹理销毁，池先销毁引用它们的 framebuffer/绑定。

update_texture、复制回读和新增清屏命令支持选定 mip/layer/aspect。复制使用单 mip、单 aspect 的紧密排列平面，验证容量与偏移。GPU 回归检查独立 mip/layer 的不同颜色、范围上传、部分覆盖及 depth/stencil 平面。

## Consequences

- 整资源图与单 mip/layer 调用保持兼容，局部使用不会改变未涉及子资源的状态。
- 分配与租约仍按完整物理对象管理；此步不引入子资源显存别名。
- depth/stencil 分离受设备特性约束，执行失败按既有 bool 返回路径处理。

## References

- [ADR-0016](ADR-0016-frame-graph-resource-versions.md)、[ADR-0017](ADR-0017-frame-graph-transient-resources.md)
- [Khronos image memory barriers](https://docs.vulkan.org/refpages/latest/refpages/source/VkImageMemoryBarrier.html)
- [Khronos copies and depth/stencil planes](https://docs.vulkan.org/spec/latest/chapters/copies.html)
