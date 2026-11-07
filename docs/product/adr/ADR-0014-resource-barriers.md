# ADR-0014: 资源访问声明与单队列自动 barrier

- **Status**: Accepted
- **Date**: 2026-11-07
- **Deciders**: core team

**后续决策**：2026-11-07 的 [ADR-0016](ADR-0016-frame-graph-resource-versions.md) 引入显式版本链，依赖区分逻辑版本，barrier 继续按物理资源累计。

## Context

ADR-0013 完成 CPU 依赖排序，但读写声明没有用途信息，无法推导 GPU 访问范围与图像布局。Vulkan render pass 原先把所有颜色附件转为呈现布局，离屏渲染无法正确衔接采样或复制。资源还会跨帧复用，仅记录图内状态无法处理首用与重用。

## Decision

在 core 引入后端无关的 `ResourceAccess`、`BufferBarrier` 与 `TextureBarrier`，为 `GraphicsDevice` 增加两个返回 `bool` 的纯虚方法：`cmd_resource_barriers` 批量请求资源目标状态，`cmd_copy_texture_to_buffer` 提供紧密排列的二维 RGBA8 读回。Vulkan 与 mock 均实现这两个方法，同步接口禁止空实现。非法句柄、用途、命令状态或不支持的复制返回 `false`，调用方停止依赖该操作的后续工作。

Frame Graph 的 `read/write` 声明资源用途。读取默认为 `ShaderRead`；写入默认为 `Automatic`，颜色附件推断为 `ColorAttachment`，其他资源推断为 `StorageWrite`。颜色附件隐式声明写入，深度资源通过 `write(id, DepthStencilAttachment)` 显式声明。每个资源保持单生产者，同一 pass 内使用一种用途；同 pass 反馈、冲突用途和别名导入均拒绝。

编译沿拓扑顺序跟踪整个资源，为首次使用、用途变化和写后访问生成 barrier，相同只读用途复用已有状态。`set_final_access` 声明导出状态，在全部 pass 成功记录后转换。`execute()` 返回 `bool`；打开命令列表、同步或导出失败时返回 `false`，保留已记录命令。

驱动持续跟踪物理资源，按记录顺序解析目标状态，用 `vkCmdPipelineBarrier` 转换整个缓冲或纹理。新建图像从 Undefined 开始，上传、附件使用和呈现均维护状态；图中首次访问从实际源状态转换，保留已有内容。shader 访问使用保守的 ALL_COMMANDS 阶段，其余映射到 vertex input、transfer、color output、early/late fragment tests 和 host 等阶段。

render pass 的附件初末布局统一为 attachment optimal，驱动在 begin render pass 前转换 framebuffer 的全部实际附件，兼容直接使用设备接口的调用方。交换链在 end_frame 转为 Present；离屏图像保持附件布局，直到后续用途触发转换。提交等待 image_available 使用 ALL_COMMANDS，以覆盖首次布局转换。

记录必须在同一线程、同一 graphics queue 上按提交顺序串行进行，每次打开一个命令列表。有未提交命令时，禁止同步上传插队。

读回接受单 mip、单层二维 R8G8B8A8_UNORM/SRGB 图像，验证缓冲容量、用途和状态。复制后将缓冲导出为 HostRead，提交并调用 wait_idle 后再 map；GPUToCpu 映射使非一致缓存失效。该接口用于 GPU 数据验证和后续截图路径。

## Consequences

- 图可自动衔接颜色/深度写入、shader 读取、storage、transfer 以及缓冲读取用途；跨帧实际布局由驱动持续跟踪。
- 调用方声明实际用途，并保证资源创建 flags 匹配。原生 API 修改和直接 map 写入由调用方维护同步与状态一致性。
- 当前跟踪模型为整个资源、单线程、单 graphics queue；子资源、异步上传、瞬态分配和资源版本列为后续扩展。
- CPU 读取前等待提交完成并处理非一致缓存。新资源从写入开始，禁止读取 Undefined 状态的未初始化资源。
- 旧 render pass 隐式布局行为被显式转换替代，须同时回归 triangle 与 mesh 路径。

## References

- [ADR-0013](ADR-0013-frame-graph-compilation.md)：本决策扩展其中自动 barrier 的边界，单生产者语义继续有效。
- [Frame Graph](../../dev/FRAME_GRAPH.md)、[core 接口](../../dev/CORE.md)、[Vulkan 驱动](../../dev/DRIVERS.md)
- 本地 Godot `servers/rendering/rendering_device_graph.cpp`：资源 usage/access 跟踪与 barrier 分组。
- Khronos [同步规范](https://docs.vulkan.org/spec/latest/chapters/synchronization.html) 与 [同步示例](https://docs.vulkan.org/guide/latest/synchronization_examples.html)。
