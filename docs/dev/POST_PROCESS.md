# 离屏场景与后处理

mesh demo 将 ECS 场景渲染到 HDR 纹理，再应用曝光与 Reinhard 色调映射，最后呈现到交换链。实现依据 [ADR-0015](../product/adr/ADR-0015-offscreen-post-process.md)，所有 shader 源位于 [内置 shader](../../main/builtin_assets/shaders/README.md)。

## 运行

在仓库根目录完成构建后运行：

```bash
./bin/debug/rover --scene main/builtin_assets/scenes/demos/primitives.rscene
./bin/debug/rover --scene main/builtin_assets/scenes/demos/primitives.rscene --exposure 2
./bin/debug/rover --scene main/builtin_assets/scenes/demos/primitives.rscene --tone-map none
```

| 参数 | 行为 |
|------|------|
| `--exposure value` | 有限正数，默认 1 |
| `--tone-map reinhard\|none` | 默认 Reinhard；none 将曝光后的值限制在 0–1 |
| `--frames count` | 正整数，达到帧数后退出 |
| `--verify-rendering` | 每帧检查 RGBA8 回读的背景、几何与 alpha |
| `--capture path.ppm` | 保存最后一帧的映射结果；未指定帧数时运行一帧 |
| `--resize-test` | 第二帧后请求 960×540，并验证目标尺寸及代数发生变化；默认 8 帧，至少 6 帧 |

这些后处理参数用于 mesh demo。PPM 保存线性 RGB 字节，与 RGBA8 中间目标数值一致；窗口通过 sRGB 交换链附件编码显示。回读与截图启用时会等待 GPU 完成。

`--verify-rendering` 适用于当前五个示例：画面应同时包含背景与几何。检查使用背景 `(0.05, 0.07, 0.10)` 经曝光和映射后的期望值，允许每通道 2 个字节单位的误差，并要求所有 alpha 为 255。这一检查覆盖离屏写入与采样链；复杂场景的语义正确性仍需对应图像或专项断言。

## Pass 与同步

```mermaid
flowchart LR
    ECS[World 与相机 UBO] --> HDR[forward：RGBA16F + D32]
    HDR --> Tone[tone_map：曝光与 Reinhard → 线性 RGBA8]
    Tone --> Present[present：采样 → 交换链]
    Tone --> Copy[可选 readback：复制 → HostRead]
```

`PostProcessPipeline::add_passes` 声明瞬态目标、绑定和 render target，按 present、tone_map、forward、可选 readback 的顺序登记；Frame Graph 编译后按 forward → tone_map → present → readback 执行。图中声明 HDR/LDR 颜色写入与采样、深度附件、相机 UniformRead、网格 VertexRead/IndexRead，以及回读 TransferRead/TransferWrite/HostRead。自动 barrier 在 pass 前完成布局与访存转换。

HDR 使用 RGBA16F，深度使用 D32。色调映射 pass 采样 HDR，计算 `color * exposure`，启用 Reinhard 时输出 `color / (1 + color)`。RGBA8 输出同时供呈现和读回使用。纹理与采样器独立绑定，对应 Vulkan SAMPLED_IMAGE / SAMPLER。

## 所有权与尺寸变化

| 位置 | 职责 |
|------|------|
| `services/graphics/post_process_pipeline.{h,cpp}` | 静态 shader/pipeline/render pass，声明 HDR/depth/LDR 与可选回读资源，持有持久资源池 |
| `services/graphics/transient_resource_pool.{h,cpp}` | 瞬态分配与使用区间复用，按完成提交回收，缓存 framebuffer/bind group |
| `main/mesh_demo.cpp` | World、网格缓存、图内 CpuToGpu 相机 UBO、场景绘制回调与运行验证 |
| `platform/linux/window.{h,cpp}`、`event_pump.cpp` | SDL 窗口与 drawable 像素尺寸事件 |
| `drivers/vulkan/graphics_device_vulkan_frame.cpp` | begin_frame 尺寸检测、交换链获取和重建 |
| `misc/cmake/RoverShader.cmake` | Slang → SPIR-V → C++ 头文件；依赖本地 slangc target |

调用顺序为 `begin_frame → prepare → add_passes → compile → prepare_resources → 更新相机缓冲 → execute → end_frame/present`；启用回读时随后调用 `read_pixels`。图编译确定使用区间，资源池准备物理资源和绑定，main 在执行前更新池提供的安全相机缓冲。

`prepare` 检查尺寸、格式和交换链图像句柄；稳定帧复用静态配置。配置变化时先建立候选呈现 pipeline/render pass，成功后等待设备空闲、清理旧池，再切换配置。动态资源分配在 `prepare_resources` 中事务执行，失败清理本次候选对象并保留既有池。

窗口尺寸以 drawable 像素为单位，零尺寸跳过帧。Vulkan 以 frame fence 推进完成提交序号，池只复用已完成资源；GPU-only 资源可在同一图内按不相交区间复用，主机可见缓冲保留整帧租约。`clear()` 拒绝清理当前已记录但未提交的资源。关闭时先释放 mesh pipeline，再清理后处理池与静态配置，最后释放 mesh 布局和 shader，设备最后关闭。

详见 [瞬态资源](TRANSIENT_RESOURCES.md)。

## 验证

```bash
ctest --test-dir build --output-on-failure
VK_LAYER_VALIDATE_SYNC=1 ./bin/debug/rover \
  --scene main/builtin_assets/scenes/demos/primitives.rscene \
  --frames 8 --verify-rendering --capture /tmp/rover-primitives.ppm
VK_LAYER_VALIDATE_SYNC=1 ./bin/debug/rover \
  --scene main/builtin_assets/scenes/demos/primitives.rscene \
  --frames 8 --verify-rendering --resize-test
```

C++ 测试覆盖提交完成后的复用、在途隔离、使用区间、主机缓冲保护、稳定帧缓存、尺寸/格式/图像集变化、逐分配点回滚、依赖销毁顺序、图排序与读回导出。Python 测试覆盖 SPIR-V 头文件生成、错误输入与源发现。GPU 验证使用五个内置场景、曝光 0.5/1/2、映射开关和尺寸重建；现有 triangle 与 frame-graph demo 用于回归。

2026-11-07：Intel Iris Xe / Vulkan 1.3 上，五个场景各 8 帧通过像素回读；曝光 0.5、1、2 的平均 RGB 分别为 18.929、33.279、54.634，关闭 Reinhard 为 44.303。缩放从 1280×720 到 960×540，目标代数从 1 到 2，缩放后六帧均通过回读。triangle 和 frame-graph 各 8 帧通过。上述运行无 Vulkan 验证错误或 SYNC-HAZARD；环境中的 OBS 隐式层产生一条既有扩展类型警告。七个生成的 SPIR-V 均通过 `spirv-val --target-env vulkan1.3`。

Frame Graph 已支持显式版本与覆盖依赖，见 [资源版本](FRAME_GRAPH.md#同一物理资源的多次写入)。

## 后续工作

- 基于已支持的 [mip/layer/aspect 子资源](TEXTURE_SUBRESOURCES.md) 增加更多后处理 pass。
- 接入 shader 优化插件时一并登记源码依赖。
