# ADR-0015: Slang 编译与离屏场景后处理

- **Status**: Accepted
- **Date**: 2026-11-07
- **Deciders**: core team

## Context

Frame Graph 已验证依赖排序、布局转换和像素读回。场景仍直接写入交换链，需要通过实际 shader 采样验证离屏颜色附件与后处理之间的数据传递。现有 shader 构建使用 GLSL，源语言迁移按 ADR-0007 推进。

## Decision

所有引擎 shader 源迁移到 `main/builtin_assets/shaders/`，使用 Slang。按 ADR-0007 与 ADR-0008，将固定版本的 Slang 源码及必要依赖纳入 `vendor/slang/`，由本地 CMake 构建宿主编译器 `slangc`。源码来源、版本、提交与校验值写入 vendor manifest；配置与构建阶段离线执行，禁止自动拉取子模块或预编译工具。Slang 用于构建 shader，运行时不链接编译器。

Slang 选择源入口并生成 Vulkan SPIR-V，保留 SPIR-V 入口 `main`。编译使用列主序矩阵，与 `Mat4` 和列向量乘法一致；依赖文件跟踪 shader import/include，Python 工具将 SPIR-V 转为内联 C++ 头文件。现有 GLSL 文件一并迁移。

新增 `services/graphics/PostProcessPipeline`，通过抽象 GraphicsDevice 管理三段渲染：

1. Forward 场景写入 RGBA16F HDR 颜色与 D32 深度。
2. 全屏 shader 采样 HDR，应用曝光和可切换 Reinhard 色调映射，输出线性 RGBA8。
3. 呈现 shader 采样 RGBA8 输出到交换链，sRGB 交换链由附件格式完成编码。

中间 RGBA8 纹理同时作为确定的像素读回目标。验证时增加复制 pass，将输出缓冲导出 HostRead，在提交完成后检查背景与几何像素，并可保存 PPM 图像。该路径使用已有 Frame Graph 与读回接口。

渲染目标和相机 UBO 按交换链图像组织，获取图像后更新对应 UBO。窗口尺寸、格式或图像句柄变化时等待 GPU 完成，创建替换资源后切换，失败清理候选资源。释放顺序为绑定与 framebuffer、纹理与缓冲、pipeline、layout、shader 和 render pass；全部资源早于设备关闭释放。

WindowSystem 尺寸以 drawable 像素为单位，SDL 像素尺寸事件更新缓存。Vulkan 在 begin_frame 比较窗口像素尺寸与上次创建交换链时的请求尺寸，变化时重建；零尺寸窗口跳过帧。该路径与 acquire/present 的过期通知共同触发重建。

main 负责 ECS 场景、命令行参数与绘制回调，services 负责后处理资源和 Frame Graph pass 声明。图中显式声明颜色、深度、相机、网格缓冲和采样用途。

纹理和采样器按 ADR-0010 的独立绑定契约使用。Vulkan 的 `SampledTexture` 映射为 SAMPLED_IMAGE，`Sampler` 映射为 SAMPLER，与 Slang 的 Texture2D / SamplerState 对应。

## Consequences

- 真实场景走完整的离屏写入、shader 采样与呈现链；已有五个内置场景复用相同入口。
- 曝光与映射开关可用于画面对比。中间纹理增加一次全屏呈现 pass 和相应显存开销。
- GPU 读回与 PPM 输出用于验证，启用时等待 GPU 完成；常规渲染保留多帧运行方式。
- 新增窗口缩放验证入口，检查交换链重建、尺寸更新与资源销毁。

## References

- [ADR-0007](ADR-0007-shader-source-language.md)、[ADR-0014](ADR-0014-resource-barriers.md)
- [Slang SPIR-V 入口与绑定](https://shader-slang.org/slang/user-guide/spirv-target-specific)
- [Slang 命令行参数](https://github.com/shader-slang/slang/blob/master/docs/command-line-slangc-reference.md)
