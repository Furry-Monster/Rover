# Phase 2 验收

2026-11-07：Phase 2 已完成。ECS 场景可保存到 JSON/RBIN，重启后恢复网格并渲染；FrameGraph 提供版本依赖、子资源同步、瞬态资源与在途帧隔离。依据 [路线图](../product/ROADMAP.md) 和 [需求](../product/REQUIREMENTS.md) 验收。

## 验收清单

| 项目 | 完成证据 | 状态 |
|------|----------|------|
| ECS、场景树、相机与光照，JSON/RBIN 保存加载并恢复网格 | 场景/序列化真实文件往返测试；五个场景 GPU 回读 | 通过 |
| Variant、Callable、文件系统抽象 | core/variant、core/os 测试；F-111/F-208/F-209 已同步 | 通过 |
| 绑定、push constant、staging、纹理上传 | 真实 mesh/shader 使用；subresources 上传与采样回读 | 通过 |
| FrameGraph DAG、版本覆盖、自动同步 | 全部 120 种五 pass 排列；双版本 GPU 回读；局部版本继承与重叠依赖测试 | 通过 |
| 瞬态纹理/缓冲、使用区间与资源池 | [资源池契约](TRANSIENT_RESOURCES.md)；32 帧区间复用与 120 帧在途 GPU 回归；失败回滚测试 | 通过 |
| mip/layer/aspect 子资源跟踪 | [子资源契约](TEXTURE_SUBRESOURCES.md)；独立生产者/状态/导出/视图缓存测试；3 mip × 2 layer 与 depth/stencil GPU 回读 | 通过 |
| Vulkan 头文件 vendoring | manifest 固定 1.4.347；禁用 FindVulkan 且无效 SDK 路径的配置通过；driver 使用 vendored 头文件，services 不依赖 Vulkan | 通过 |
| F-605 lint 命令 | 5 项 CLI/编译数据库测试；实际 clang-tidy 调用与失败码传播 | 通过 |
| 工程验收 | Debug/Release 完整构建与测试；Clang ASan/UBSan/泄漏检查；两种构建各 13 项 GPU 回归 | 通过 |
| 里程碑与需求同步 | Roadmap、Requirements、架构、驱动、图与测试文档同步 | 完成 |

外部模型/图片导入、材质与纹理的磁盘引用、磁盘资产注册表和编辑器按既有路线图在 Phase 3 完成。F-406 跨 v0.2/v0.3，场景保存加载部分已验收，外部资产 import/export 继续保留 Accepted。显存别名、异步队列、pass 剔除和 shader 优化为后续增强。

## 验证环境与结果

- 系统：Nobara Linux 44，Linux x86_64；CMake 4.3.0。
- 常规编译：GCC 16.2.1；sanitizer：Clang 22.1.8，ASan 与 UBSan 同时启用，`detect_leaks=1`。
- GPU：Intel Iris Xe Graphics（RPL-U），Vulkan 1.3；启用 Khronos 验证层和同步验证。

| 配置 | 构建范围 | C++ | Python | GPU |
|------|----------|-----|--------|-----|
| Debug，`build/` | 完整默认构建，含本地 Slang 与 shader | 194 cases / 3179 assertions | 14 项 | 13/13 |
| Release，`build/release/` | 完整默认构建，含本地 Slang 与 shader | 194 cases / 3179 assertions | 14 项 | 13/13 |
| ASan + UBSan，`build/sanitizers/` | rover_tests 及其引擎依赖，独立输出目录 | 194 cases / 3179 assertions | 14 项 | 由上两行验证 |

Sanitizer 未报告内存越界、未定义行为或泄漏；vendor 库沿用上游编译选项。GCC sanitizer 链接因系统缺少 libasan 失败后，使用本机完整的 Clang 运行库完成验收。LeakSanitizer 在沙箱内无法检查线程，最终测试在沙箱外通过。

GPU 回归均无 Vulkan 验证错误、VUID 或 SYNC-HAZARD。验证层启动时的环境层提示、不可用 ICD 提示和本机注入层的 extension 警告保留在日志中。13 项覆盖五个场景、双版本与物理纹理复用、子资源、triangle、120 帧在途、窗口缩放、两档曝光和关闭色调映射。FrameGraph 32 帧使用 1 张纹理及 3 个回读缓冲；mesh 在途运行保持 6 张中间纹理及 2 个相机缓冲。

## 重现命令

```bash
CCACHE_DISABLE=1 cmake --build build -j4
ctest --test-dir build --output-on-failure
CCACHE_DISABLE=1 cmake --build build/release -j4
ctest --test-dir build/release --output-on-failure
python3 misc/scripts/phase2_gpu_regression.py --exe bin/debug/rover --output-dir /tmp/rover-phase2-gpu-final-debug
python3 misc/scripts/phase2_gpu_regression.py --exe bin/release/rover --output-dir /tmp/rover-phase2-gpu-final-release
```

干净配置与 Vulkan 依赖隔离检查：

```bash
VULKAN_SDK=/tmp/rover-no-sdk cmake -S . -B /tmp/rover-vendored-headers-check -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DROVER_TESTS=OFF -DROVER_EDITOR=OFF \
  -DCMAKE_DISABLE_FIND_PACKAGE_Vulkan=TRUE
```

该配置的编译数据库中，13 个 driver 翻译单元均获得 `vendor/Vulkan-Headers/include`，6 个 services 翻译单元均未获得该路径。Sanitizer 构建与运行命令见 [测试规范](../standard/TESTING.md#74-asan--ubsan)。

clang-tidy 命令及选项见 [工具说明](MISC.md#clang-tidy-与-phase-2-gpu-回归)。检查图、资源池、子资源 demo 与 Vulkan 改动涉及的 11 个翻译单元。全量 analyzer 的严格 warnings-as-errors 检查仍报告已有析构期虚函数调用、Debug 分支无效初始化及 flags 枚举组合误报；常规检查退出码为 0 并保留这些诊断，后续按工程清理处理。

本轮本机日志位于 `/tmp/rover-phase2-final-debug-ctest.log`、`/tmp/rover-phase2-final-release-ctest.log`、`/tmp/rover-phase2-sanitizers-ctest.log`、`/tmp/rover-phase2-final-analyzer-warnings.log`；两份 GPU 报告位于上述 output-dir 的 `report.json`。日志为本机临时产物，仓库中的脚本和命令用于重新生成。

## 资源生命周期原则

- 池中的资源只有在其最后使用提交完成后，才能交给下一帧。
- 同一图内描述相同、使用区间不相交的 GPU 资源可以复用同一物理对象；主机可见缓冲保留整帧租约。
- framebuffer 与 bind group 由池缓存，清理早于引用的纹理/缓冲和布局。
- 分配或绑定创建失败，撤销候选资源，已存在的池对象与旧渲染配置保持有效。
- 设备关闭前等待 GPU，并按绑定、framebuffer、资源、pipeline/layout/render pass 的依赖顺序释放。
