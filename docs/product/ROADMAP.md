# Rover 引擎路线图

> 本文件维护**全局**的演进规划与支持矩阵。子系统级 TODO 列表见各 [`docs/dev/*.md`](../dev/) 文件末尾。新增/变更条目时同步更新需求清单 [`REQUIREMENTS.md`](REQUIREMENTS.md) 与相关 [ADR](adr/INDEX.md)。

---

## 1. 总体里程碑

| 版本 | 主题 | 状态 | 完成时间 |
|------|------|------|---------|
| **v0.1.0** | Phase 1 — 核心系统 + Vulkan 三角形 | ✅ Done | 2026-04 |
| **v0.2.0** | Phase 2 — Frame Graph + ECS 场景 + 资源管线 | 🚧 Planned | — |
| **v0.3.0** | Phase 3 — 编辑器 GUI/CLI + Asset Browser | ⏳ Planned | — |
| **v0.4.0** | Phase 4 — 物理 / 音频 / 网络服务 | ⏳ Planned | — |
| **v0.5.0** | Phase 5 — 跨平台铺开（Windows / macOS） | ⏳ Planned | — |
| **v0.6.0** | Phase 6 — 第二个渲染后端（D3D12 或 Metal） | ⏳ Planned | — |
| **v1.0.0** | Phase 7 — 移动端 + Web，正式发布 | ⏳ Planned | — |

---

## 2. 支持矩阵

### 2.1 渲染 API × 平台

| API \ 平台 | Linux | Windows | macOS | Android | iOS | Web |
|-----------|:-----:|:-------:|:-----:|:-------:|:---:|:---:|
| **Vulkan**       | ✅ v0.1 | 🎯 v0.5 | 🟡 MoltenVK 评估中 | 🎯 v1.0 | 🟡 MoltenVK | ❌ |
| **D3D12**        | ❌     | 🎯 v0.6 | ❌    | ❌     | ❌  | ❌  |
| **Metal**        | ❌     | ❌      | 🎯 v0.6 | ❌   | 🎯 v1.0 | ❌ |
| **WebGPU**       | 🟡 评估 | 🟡 评估 | 🟡 评估 | ❌  | ❌  | 🎯 v1.0 |
| **OpenGL ES 3**  | ❌     | ❌      | ❌    | 🟡 备选 | 🟡 备选 | ❌ |

**图例**：
- ✅ 已实现并测试通过
- 🎯 已规划并写明里程碑
- 🟡 评估中 / 候补方案
- ❌ 不做或暂无计划

**指导原则**：
- **Vulkan 是优先后端**：原生支持 Linux/Windows/Android；macOS/iOS 通过 MoltenVK 验证可行性后再决定是否加 Metal
- **D3D12** 仅在 Vulkan-on-Windows 性能不达标或调试体验显著差时优先级提升
- **WebGPU** 是 Web 平台的唯一现实选择；wgpu / Dawn 都在评估中
- **OpenGL ES 3** 只作为不支持 Vulkan 的极老 Android 设备的回退方案，非核心目标

### 2.2 子系统能力矩阵

| 子系统 | v0.1 | v0.2 | v0.3 | v0.4 | v0.5 | v1.0 |
|-------|:----:|:----:|:----:|:----:|:----:|:----:|
| **core**（log/math/event/task/object/allocator/graphics 抽象） | ✅ | + Variant 完善 | + IO 抽象 | | | |
| **services/graphics** | — | 🎯 Frame Graph | + 材质系统 | + GPU culling | | |
| **services/physics** | — | — | — | 🎯 刚体 + 碰撞 | + 软体 / 布料 | |
| **services/audio** | — | — | — | 🎯 混音 + 空间音频 | | |
| **services/network** | — | — | — | 🎯 传输 + 复制 | + RPC | |
| **modules/scene** | — | 🎯 EnTT World + 场景树 | + 序列化 | | | |
| **modules/animation** | — | — | 🎯 骨骼 / 状态机 | + 混合树 | | |
| **modules/particle** | — | — | 🎯 CPU 粒子 | + GPU 粒子 | | |
| **modules/ui** | — | — | 🎯 基础控件 | + 主题 | | |
| **modules/ai** | — | — | — | 🎯 导航 / 行为树 | + 黑板 | |
| **modules/serialization** | — | 🎯 二进制 + 文本格式 | + 资源压缩 | | | |
| **editor/gui** | — | — | 🎯 ImGui 主框架 + Inspector | + Asset Browser | | |
| **editor/cli** | — | — | 🎯 import/export/build | + 节点编辑命令 | | |

---

## 3. Phase 详解

### Phase 1 — v0.1 (✅ Done)

**目标**：一个最小但分层正确的引擎，能在 Linux 上启动并渲染彩色三角形。

完成项：
- ✅ core 全部子系统：log / math / allocator / event / task / object / variant / time / typedefs
- ✅ core/graphics 抽象：`GraphicsDevice` + `WindowSystem` 纯虚接口
- ✅ drivers/vulkan：完整 Vulkan 1.3 实现（volk + VMA）
- ✅ platform/linux：SDL3 窗口、事件、计时
- ✅ main：装配 + 主循环 + 三角形 demo
- ✅ tests：core 单元测试（75 cases / 255 assertions）
- ✅ misc/scripts/rover-cli：工程化 Python 开发者 CLI
- ✅ misc/cmake：构建模块（Options / Compiler / Utils / Shader）
- ✅ docs：架构 + 各子系统权威文档

### Phase 2 — v0.2 (🚧 Planned)

**目标**：把渲染从 demo 升级为可持续演进的生产路线，并建立 ECS 场景。

里程碑：
- 🎯 `services/graphics/`：Frame Graph 抽象（render passes 串成 DAG，自动 barrier）
- 🎯 `modules/scene/`：EnTT World 包装、`Entity` / `Component` 反射（基于 `core/object/`）
- 🎯 `modules/serialization/`：基础二进制 + 文本（json）格式 + Asset 注册表
- 🎯 `core/io/`：文件系统抽象 + Linux 实现（VFS 层简化为原生路径）
- 🎯 `core/variant/`：填充 Variant 实现（v0.1 仅占位）
- 🎯 渲染端：descriptor set / push constant / staging buffer 上传 / `update_texture` 完整实现
- 🎯 在 Frame Graph 之上跑一个简单的「立方体 + 相机」demo

退出条件：能用 ECS 描述一个 3D 场景，序列化到磁盘 / 加载回来，跑出至少 1 个 mesh + 1 个 light 的画面。

### Phase 3 — v0.3 (⏳ Planned)

**目标**：编辑器骨架可用，AI Agent 能通过 CLI 完成基本编辑流程。

里程碑：
- 🎯 `editor/gui/`：ImGui 主循环 + Dock + Inspector + Asset Browser + Scene Tree
- 🎯 `editor/cli/`：`import / export / build / run / list` 命令族
- 🎯 `modules/animation/`：骨骼 + 状态机（无混合树）
- 🎯 `modules/particle/`：CPU 粒子系统
- 🎯 `modules/ui/`：基础控件（button / label / panel / image）
- 🎯 测试：services / drivers 集成测试，`GraphicsDevice` mock

退出条件：能在编辑器中导入一个 GLTF 模型，把它放到场景里，运行时正确显示。

### Phase 4 — v0.4 (⏳ Planned)

**目标**：四大重型服务完整。

里程碑：
- 🎯 `services/physics/`：刚体 + 碰撞（评估 Jolt Physics vs Bullet）
- 🎯 `services/audio/`：混音 + 空间音频（评估 miniaudio vs OpenAL Soft）
- 🎯 `services/network/`：传输（UDP/TCP）+ 复制 + RPC
- 🎯 `modules/ai/`：导航 + 行为树
- 🎯 编辑器：每个服务对应 GUI 工具

### Phase 5 — v0.5 (⏳ Planned)

**目标**：Vulkan-on-Windows、Vulkan-on-macOS（MoltenVK）通跑。

里程碑：
- 🎯 `platform/windows/`：Win32 实现，Vulkan 后端通过
- 🎯 `platform/mac/`：Cocoa 实现，MoltenVK 验证
- 🎯 CMake / 工具链：跨平台编译验证
- 🎯 CI：GitHub Actions 三平台矩阵构建

### Phase 6 — v0.6 (⏳ Planned)

**目标**：第二个渲染后端落地。

需在 ADR 中先决定：D3D12 vs Metal 优先做哪个。当前倾向 **D3D12**（覆盖 Windows + Xbox 路线，且 RenderDoc 调试支持成熟），Metal 可能借助 MoltenVK 推迟。

里程碑：
- 🎯 选定后端的 ADR
- 🎯 `drivers/<api>/`：完整实现 `GraphicsDevice`
- 🎯 验证：双后端在 Windows 上跑同一 demo 输出一致

### Phase 7 — v1.0 (⏳ Planned)

**目标**：移动端 + Web 可发布。

里程碑：
- 🎯 `platform/android/`、`platform/ios/`、`platform/web/`
- 🎯 Android Vulkan 后端、iOS Metal 后端、Web WebGPU 后端
- 🎯 触摸输入、控制器输入、生命周期事件（暂停/恢复）
- 🎯 资源打包格式（替代源码内联 SPIR-V）
- 🎯 文档：发布手册、第三方贡献指南

---

## 4. 不在路线图内（明确不做）

为防止 scope creep：

- ❌ **可视化脚本编辑器**（如 Godot 的 GDScript / Unreal Blueprint）：无 C# 计划；脚本接口暂为 C++ 与 CLI
- ❌ **完整渲染管线**（如 Lumen / Nanite 级别）：v1.0 范围只覆盖 forward + 简单后处理
- ❌ **VR / AR 支持**：评估时机推迟到 v1.1+
- ❌ **多人在线服务托管**：网络服务限于客户端 + 自定义服务器，不做 SaaS
- ❌ **专属 IDE 插件**：clangd 已经够用，不写自定义插件

---

## 5. 决策跟踪

每一个 🎯 / 🟡 项最终落地前都应该有 ADR 支持。当前已记录：

| 决策 | ADR |
|------|-----|
| 三层倒置架构 | [ADR-0001](adr/ADR-0001-three-layer-architecture.md) |
| 单一 CMake 构建 | [ADR-0002](adr/ADR-0002-cmake-as-single-build-system.md) |
| scene 作为模块 | [ADR-0003](adr/ADR-0003-modules-over-fixed-scene-tree.md) |
| services 命名 | [ADR-0004](adr/ADR-0004-services-vs-servers-naming.md) |
| CLI 归属 editor | [ADR-0005](adr/ADR-0005-cli-under-editor.md) |
| IO 拆分 | [ADR-0006](adr/ADR-0006-io-split-platform-and-serialization.md) |

待规划的 ADR（按 phase 触发）：

- **Phase 2**：Variant 实现策略（手写 vs `std::variant` vs SBO）、Asset Pipeline 总体架构
- **Phase 4**：物理引擎选型（Jolt vs Bullet vs PhysX）、音频引擎选型
- **Phase 6**：第二渲染后端选型（D3D12 vs Metal 优先）

---

*Rover Engine v0.1.0 — 路线图与项目同步演进。*
