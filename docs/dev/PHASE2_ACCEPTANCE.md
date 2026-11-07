# Phase 2 验收

目标：完成 v0.2 的 ECS 场景、保存加载、GPU 渲染及资源管线，并完成已列入 Phase 2 的工具与工程收尾。依据 [路线图](../product/ROADMAP.md)、[需求](../product/REQUIREMENTS.md) 和资源版本完成后的开发顺序逐项验收。

## 验收清单

| 项目 | 完成证据 | 状态 |
|------|----------|------|
| ECS、场景树、相机与光照，JSON/RBIN 保存加载并恢复网格 | 场景/序列化真实文件往返测试及五个场景 GPU 回归 | 已有实现，最终复验 |
| Variant、Callable、文件系统抽象 | 对应 core/platform 测试；同步 F-111/F-208/F-209 状态 | 已有实现，最终复验 |
| 绑定、push constant、staging、纹理上传 | driver 与模块测试及真实 shader/上传 GPU 使用 | 已有实现，最终复验 |
| Frame Graph DAG、版本覆盖、自动同步 | 版本排列测试与双版本 GPU 回读 | 已有实现，最终复验 |
| 瞬态纹理/缓冲、使用区间与资源池 | `TRANSIENT_RESOURCES.md`；池/后处理测试；32 帧物理复用与 120 帧在途 GPU 回归 | 已完成，最终矩阵复验 |
| mip/layer/aspect 子资源跟踪 | 不同范围独立状态、重叠范围依赖、整资源兼容性、实际 GPU 子资源验证 | 待完成 |
| Vulkan 头文件 vendoring | manifest 固定 1.4.347；禁用 FindVulkan 且无效 SDK 路径的干净配置通过；Debug 构建及 GPU 回归通过 | 已完成，最终矩阵复验 |
| F-605 lint 命令 | 5 项 CLI/编译数据库测试；实际 clang-tidy 检查池与 FrameGraph，无编译错误；现有风格警告保留 | 已完成 |
| 工程验收 | Debug/Release 构建、单元测试、ASan/UBSan、可重复 GPU 回归脚本，记录环境和结果 | 待完成 |
| 里程碑与需求同步 | 上述证据逐项核验后更新 Roadmap 和 Requirements | 待完成 |

外部模型/图片导入、材质与纹理的磁盘引用、磁盘资产注册表和编辑器属于路线图中的 Phase 3。显存别名、异步队列、pass 剔除和 shader 优化作为后续增强。

## 资源生命周期原则

- 池中的资源只有在其最后使用提交完成后，才能交给下一帧。
- 同一图内描述相同、使用区间不相交的 GPU 资源可以复用同一物理对象；主机可见缓冲保留整帧租约。
- framebuffer 与 bind group 由池缓存，清理早于引用的纹理/缓冲和布局。
- 分配或绑定创建失败，撤销候选资源，已存在的池对象与旧渲染配置保持有效。
- 设备关闭前等待 GPU，并按绑定、framebuffer、资源、pipeline/layout/render pass 的依赖顺序释放。

## 2026-11-07 中间验收

- Debug：`ctest --test-dir build --output-on-failure` 通过，185 个 C++ 测试与 13 个 Python 测试。
- GPU：`python3 misc/scripts/phase2_gpu_regression.py --exe bin/debug/rover --output-dir /tmp/rover-phase2-gpu-debug`，12 项全部通过；报告及日志保存在指定目录。
- 干净配置：`VULKAN_SDK=/tmp/rover-no-sdk cmake -S . -B /tmp/rover-vendored-headers-check -G Ninja -DCMAKE_BUILD_TYPE=Debug -DROVER_TESTS=OFF -DROVER_EDITOR=OFF -DCMAKE_DISABLE_FIND_PACKAGE_Vulkan=TRUE` 通过。编译数据库确认 driver 使用 vendored 头文件，services 未获得 Vulkan include 路径。
- Release 宿主编译器正在 `build/release` 构建；最终 Release/ASan/UBSan 验收在子资源实现后进行。
- 尚未验收：mip/layer/aspect 子资源、最终构建矩阵与需求状态同步。Phase 2 保持进行中。
