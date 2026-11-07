# ADR-0017: Frame Graph 瞬态资源与提交完成回收

- **Status**: Accepted
- **Date**: 2026-11-07
- **Deciders**: core team

## Context

图已经描述访问与版本，但后处理中间目标和相机缓冲仍由调用方逐图像管理。资源复用需要依据编译后的使用区间及 GPU 完成状态，同时处理 framebuffer、bind group 的依赖生命周期。

## Decision

FrameGraph 增加瞬态纹理/缓冲声明、图内 render target 与 bind group 声明。compile 生成每个物理根资源的首次/末次使用区间；版本共享区间，导出延长至图结束。GPU-only 瞬态资源需要生产者，CpuToGpu 缓冲允许在资源准备后、图执行前由调用方初始化。

持久的 TransientResourcePool 通过抽象 GraphicsDevice 创建与缓存资源。prepare_resources 将编译计划解析成物理句柄，并准备 framebuffer 和 bind group；成功后提交到图，失败撤销候选资源。资源描述中的 debug_name 不参与复用比较。

GraphicsDevice 暴露当前记录提交序号与已完成提交序号。Vulkan 在同一 graphics queue 提交时递增序号，通过 frame fence 或 wait_idle 推进完成序号。池根据完成序号复用跨帧资源，不以获取交换链图像代替 CPU 可见的 GPU 完成证明。

图内描述相同且使用区间不相交的 GPU-only 资源可以复用同一对象。主机可见缓冲保留整帧租约，避免提交前的 CPU 更新覆盖先前 pass 所需数据。framebuffer 与 bind group 是不可变缓存对象。池清理等待设备空闲，先释放绑定和 framebuffer，再释放资源；调用方随后释放 render pass、pipeline layout 与绑定布局。

PostProcessPipeline 使用该池分配 HDR/depth/LDR、可选读回缓冲和绑定。main 将相机 UBO 声明为图内 CpuToGpu 缓冲，准备后更新，再执行图。尺寸、格式和交换链图像集变化时清空旧动态资源，保留静态渲染配置的事务式替换。

## Consequences

- 稳定帧按描述复用资源，GPU 在途数据受提交序号保护。
- 图资源在准备后获得句柄；跨帧保留内容继续使用 import。
- 复用物理对象时，由 driver 的持续状态生成实际源 barrier；本阶段不绑定不同 VkImage 到同一显存区间。

## References

- [ADR-0016](ADR-0016-frame-graph-resource-versions.md)
- [Phase 2 验收](../../dev/PHASE2_ACCEPTANCE.md)
