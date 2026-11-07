# ADR-0014: 资源访问声明与单队列自动 barrier

- **Status**: Accepted
- **Date**: 2026-11-07
- **Deciders**: core team

## Context

ADR-0013 完成 CPU 依赖排序，但读写声明没有用途信息，无法推导 GPU 访问范围与图像布局。Vulkan render pass 原先把所有颜色附件转为呈现布局，离屏渲染无法正确衔接采样或复制。资源还会跨帧复用，仅记录图内状态无法处理首用与重用。

## Decision

在 core 引入后端无关的 `ResourceAccess`、`BufferBarrier` 与 `TextureBarrier`，为 `GraphicsDevice` 增加两个返回 `bool` 的纯虚方法：`cmd_resource_barriers` 批量请求资源目标状态，`cmd_copy_texture_to_buffer` 提供紧密排列的二维 RGBA8 读回。Vulkan 与 mock 均实现；不提供静默跳过同步的默认实现。非法句柄、用途、命令状态或不支持的复制返回 `false`，调用方停止依赖它的操作。

Frame Graph 的 `read/write` 可声明具体用途，默认读取为 `ShaderRead`，默认写入为 `Automatic`（颜色附件推断为 `ColorAttachment`，其他资源为 `StorageWrite`）。颜色附件隐式使用 `ColorAttachment`；深度资源通过 `write(id, DepthStencilAttachment)` 显式声明。同一 pass 内同一资源只能有一种用途，冲突声明拒绝。编译沿拓扑顺序跟踪整个资源的用途，在首次使用、用途变化及写后访问时生成每 pass barrier 计划。`set_final_access` 声明导出状态，最终转换在所有 pass 成功记录后执行。保留单生产者、禁止同 pass 反馈和别名导入的约束；相同只读用途复用不增加 barrier。`execute()` 返回 `bool`，打开命令列表、同步或导出失败会返回 `false`，已记录命令不会回滚。

驱动拥有物理资源的持续状态，按实际记录顺序解析目标状态，使用 `vkCmdPipelineBarrier` 生成整个缓冲/纹理的 barrier。首次创建图像为 Undefined，上传、附件使用和呈现都维护状态。首次图访问的源状态由驱动解析，不能把它当作丢弃已有内容的 Undefined 转换。shader 访问使用保守的 ALL_COMMANDS 阶段，其余映射到 vertex input、transfer、color output、early/late fragment tests、host 等阶段。

render pass 的附件初末布局统一为 attachment optimal，在 begin render pass 前由驱动转换实际 framebuffer 的全部附件；该路径兼容不使用 Frame Graph 的旧调用。end_frame 追加交换链图像到 Present 的转换，提交等待 image_available 使用 ALL_COMMANDS，以覆盖图像的首次布局转换。离屏图像不会被转成 Present。记录必须在同一线程、同一 graphics queue、按提交顺序串行进行，每次只打开一个命令列表；有未提交命令时禁止同步上传插队。

读回只支持单 mip、单层二维 R8G8B8A8_UNORM/SRGB 图像，验证容量、用途和状态；使用后将缓冲导出为 HostRead，提交后调用 wait_idle 再 map。GPUToCpu 映射使非一致缓存失效。该窄接口用于实际 GPU 数据验证，也为后续截图路径提供基础。

## Consequences

- 图可自动衔接颜色/深度写入、shader 读取、storage、transfer 以及缓冲读取用途；跨帧实际布局由驱动持续跟踪。
- 调用方必须声明实际用途并保证资源创建 flags 与用途匹配。外部原生 API 修改、直接 map 后写入、多个队列/线程、子资源、异步上传、瞬态分配与资源版本不在本轮范围内。
- CPU 读取需等待提交完成；barrier 不替代 fence 或非一致内存管理。Undefined 首用只允许写入，不能读取未初始化的新资源。
- 旧 render pass 隐式布局行为被显式转换替代，须同时回归 triangle 与 mesh 路径。

## References

- [ADR-0013](ADR-0013-frame-graph-compilation.md)：本决策扩展其中自动 barrier 的边界，单生产者语义继续有效。
- [Frame Graph](../../dev/FRAME_GRAPH.md)、[core 接口](../../dev/CORE.md)、[Vulkan 驱动](../../dev/DRIVERS.md)
- 本地 Godot `servers/rendering/rendering_device_graph.cpp` 的资源 usage/access 跟踪及 barrier 分组；采用自有小型实现，未移植子资源和多队列系统。
- Khronos [同步规范](https://docs.vulkan.org/spec/latest/chapters/synchronization.html) 与 [同步示例](https://docs.vulkan.org/guide/latest/synchronization_examples.html)。
