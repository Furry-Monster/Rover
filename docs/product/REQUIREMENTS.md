# Rover 引擎需求清单

> 维护一个 **可增减** 的需求列表，每条带稳定编号 + 状态标签。新增需求 = 新条目；废弃需求 = 状态改为 `Deprecated`，不删除条目（保留历史）。

---

## 编号规则

- **`F-NNN`** —— Functional Requirement（功能需求）
- **`NF-NNN`** —— Non-Functional Requirement（非功能需求）

NNN 一旦分配永不重用。同一编号的需求可以演化（更新描述与状态），但语义保持。

## 状态标签

| 标签 | 含义 |
|------|------|
| `Proposed` | 提出但未确认是否纳入 |
| `Accepted` | 已纳入路线图，等待实现 |
| `Implemented` | 已落地并验收 |
| `Deferred` | 暂缓（注明推迟到的版本） |
| `Deprecated` | 决定不做（注明替代方案或原因） |

---

## 1. 功能需求（Functional）

### 1.1 渲染

| ID | 描述 | 状态 | 实现版本 / 计划 | 备注 |
|----|------|------|----------------|-----|
| F-001 | 在窗口中显示彩色三角形 | Implemented | v0.1 | Phase 1 里程碑 |
| F-002 | 支持 Vulkan 1.3 后端 | Implemented | v0.1 | drivers/vulkan/ |
| F-003 | 支持窗口缩放后交换链重建 | Implemented | v0.1 | recreate_swapchain |
| F-004 | 提供 GraphicsDevice 抽象接口（与具体后端解耦） | Implemented | v0.1 | core/graphics/ |
| F-005 | 支持顶点 + 索引缓冲渲染 | Implemented | v0.1 | cmd_draw / cmd_draw_indexed |
| F-006 | 支持 SPIR-V shader 编译时内联 | Implemented | v0.1 | misc/cmake/RoverShader.cmake |
| F-007 | 实现 Frame Graph 抽象（自动 barrier） | Accepted | v0.2 | services/graphics/ |
| F-008 | 实现 descriptor set / push constant 抽象 | Accepted | v0.2 | drivers/vulkan/ + 抽象层 |
| F-009 | 支持 staging buffer 自动上传 | Accepted | v0.2 | update_buffer 对 GpuOnly |
| F-010 | 实现 update_texture 完整路径（layout transition + copy） | Accepted | v0.2 | drivers/vulkan/ |
| F-011 | 支持 D3D12 后端 | Accepted | v0.6 | 需先 ADR 决定优先级 |
| F-012 | 支持 Metal 后端 | Accepted | v0.6+ | macOS/iOS |
| F-013 | 支持 WebGPU 后端 | Accepted | v1.0 | Web 平台唯一选项 |
| F-014 | 支持 MSAA | Accepted | v0.4 | render pass 多重采样 |
| F-015 | 支持 Compute pipeline | Accepted | v0.3 | drivers/vulkan/ |

### 1.2 平台

| ID | 描述 | 状态 | 实现版本 / 计划 | 备注 |
|----|------|------|----------------|-----|
| F-101 | 支持 Linux 桌面（X11/Wayland 通过 SDL3） | Implemented | v0.1 | platform/linux/ |
| F-102 | 提供键盘 + 鼠标输入 | Implemented | v0.1 | input_events.h |
| F-103 | 提供高精度计时器 | Implemented | v0.1 | TimeSource |
| F-104 | 支持 Windows 桌面 | Accepted | v0.5 | 复用 SDL3 极简 |
| F-105 | 支持 macOS 桌面 | Accepted | v0.5 | MoltenVK 验证 |
| F-106 | 支持 Android | Accepted | v1.0 | NativeActivity |
| F-107 | 支持 iOS | Accepted | v1.0 | UIApplicationMain |
| F-108 | 支持 Web | Accepted | v1.0 | Emscripten + WebGPU |
| F-109 | 支持游戏控制器（gamepad） | Accepted | v0.3 | SDL_GAMEPAD_* |
| F-110 | 支持触摸输入 | Accepted | v1.0 | 移动端必备 |
| F-111 | 提供文件系统抽象 + 平台实现 | Accepted | v0.2 | core/os/ + platform/<os>/file.cpp |
| F-112 | 支持多窗口 | Deferred | v1.1+ | LinuxPlatform 当前持有单窗口 |
| F-113 | 支持全屏 / borderless 切换 | Deferred | v0.3 | SDL3 已支持，需暴露 |
| F-114 | 提供 DPI 缩放查询 | Deferred | v0.3 | 高 DPI 屏幕必备 |

### 1.3 核心系统

| ID | 描述 | 状态 | 实现版本 / 计划 | 备注 |
|----|------|------|----------------|-----|
| F-201 | 提供日志系统（多通道、级别过滤） | Implemented | v0.1 | core/log/，spdlog 封装 |
| F-202 | 提供数学库（向量 / 矩阵 / 四元数 / 变换） | Implemented | v0.1 | core/math/，glm 薄封装 |
| F-203 | 提供分配器（线性 / 池 / Arena） | Implemented | v0.1 | core/allocator/ |
| F-204 | 提供事件总线 + Signal/Delegate | Implemented | v0.1 | core/event/ |
| F-205 | 提供 Job 系统（work-stealing） | Implemented | v0.1 | core/task/ |
| F-206 | 提供 Object 基类 + ClassDB 反射 | Implemented | v0.1 | core/object/ |
| F-207 | 提供 RefCounted 引用计数 | Implemented | v0.1 | core/object/ref_counted.h |
| F-208 | 完善 Variant 类型系统 | Accepted | v0.2 | core/variant/ |
| F-209 | 提供 Callable（类型擦除调用包装） | Accepted | v0.2 | core/variant/ |
| F-210 | 提供时间子系统（Δt / 计时器 / 时间戳） | Implemented | v0.1 | core/time/ |

### 1.4 服务（Services）

| ID | 描述 | 状态 | 实现版本 / 计划 | 备注 |
|----|------|------|----------------|-----|
| F-301 | GraphicsService（场景渲染、材质） | Accepted | v0.2 | services/graphics/ |
| F-302 | PhysicsService（刚体、碰撞） | Accepted | v0.4 | 引擎选型待定 |
| F-303 | AudioService（混音、空间音频） | Accepted | v0.4 | 引擎选型待定 |
| F-304 | NetworkService（传输、复制、RPC） | Accepted | v0.4 | 自研或 ENet 封装 |

### 1.5 模块（Modules）

| ID | 描述 | 状态 | 实现版本 / 计划 | 备注 |
|----|------|------|----------------|-----|
| F-401 | scene 模块（EnTT World + 场景树） | Accepted | v0.2 | modules/scene/ |
| F-402 | animation 模块（骨骼、状态机） | Accepted | v0.3 | modules/animation/ |
| F-403 | particle 模块（CPU + GPU 粒子） | Accepted | v0.3+ | modules/particle/ |
| F-404 | ui 模块（布局、控件、主题） | Accepted | v0.3 | modules/ui/ |
| F-405 | ai 模块（导航、行为树、黑板） | Accepted | v0.4 | modules/ai/ |
| F-406 | serialization 模块（资产格式、import/export） | Accepted | v0.2 | modules/serialization/ |

### 1.6 编辑器

| ID | 描述 | 状态 | 实现版本 / 计划 | 备注 |
|----|------|------|----------------|-----|
| F-501 | GUI 编辑器主框架（ImGui + Dock） | Accepted | v0.3 | editor/gui/ |
| F-502 | Inspector 面板（基于反射） | Accepted | v0.3 | editor/gui/ |
| F-503 | Asset Browser | Accepted | v0.3 | editor/gui/ |
| F-504 | Scene Tree 视图 | Accepted | v0.3 | editor/gui/ |
| F-505 | Play-In-Editor 模式 | Accepted | v0.4 | editor/gui/ |
| F-506 | CLI 编辑器入口（AI Agent 友好） | Accepted | v0.3 | editor/cli/ |
| F-507 | CLI: import / export / build / run / list 命令 | Accepted | v0.3 | editor/cli/commands/ |

### 1.7 工具与构建

| ID | 描述 | 状态 | 实现版本 / 计划 | 备注 |
|----|------|------|----------------|-----|
| F-601 | rover-cli 开发者工程脚本（configure/build/run/test/...） | Implemented | v0.1 | misc/scripts/ |
| F-602 | CMake 模块化（RoverOptions/Compiler/Utils/Shader/Version） | Implemented | v0.1 | misc/cmake/ |
| F-603 | 自动 Vulkan SDK 检测与验证层启用 | Implemented | v0.1 | misc/scripts/rover/vulkan.py |
| F-604 | clang-format / clang-tidy 集成 | Implemented | v0.1 | rover format（tidy 待加） |
| F-605 | rover lint 子命令（clang-tidy） | Accepted | v0.2 | misc/scripts/ |
| F-606 | rover watch 子命令（自动增量构建） | Deferred | v0.3 | inotify/fsevents |
| F-607 | rover bench 子命令 | Deferred | v0.4 | 待 benchmark 框架 |
| F-608 | rover doc 子命令（生成 API 文档） | Deferred | v0.4 | 评估 sphinx / mkdocs |
| F-609 | shell 自动补全（bash/zsh） | Deferred | v0.3 | argcomplete |
| F-610 | CI 工作流（GitHub Actions 多平台矩阵） | Accepted | v0.5 | 跨平台时落地 |

---

## 2. 非功能需求（Non-Functional）

### 2.1 性能

| ID | 描述 | 度量 / 目标 | 状态 |
|----|------|------------|------|
| NF-001 | 引擎启动到渲染首帧时间 | < 500ms（Debug），< 200ms（Release） | Implemented |
| NF-002 | 60Hz 主循环空帧 CPU 占用 | < 1% 单核 | Implemented |
| NF-003 | 双帧 in-flight 不引入额外帧延迟 | 实测无明显延迟 | Implemented |
| NF-004 | 每帧分配（小对象）应优先用 LinearAllocator | 默认每帧 reset | Accepted |
| NF-005 | 抽象层调用开销 | 单次虚函数 ≈ 1-2ns，可接受 | Accepted |

### 2.2 可移植性

| ID | 描述 | 状态 |
|----|------|------|
| NF-101 | 任何平台相关 API 都通过 `core/` 抽象 + `platform/<os>/` 实现暴露 | Accepted |
| NF-102 | 任何 GPU 相关 API 都通过 `core/graphics/` 抽象 + `drivers/<api>/` 实现暴露 | Implemented |
| NF-103 | 不在 `services/` / `modules/` 中 include 任何具体 driver / platform 头文件 | Implemented |
| NF-104 | C++ 标准：C++20（不允许使用 C++23 特性除非启用 feature 检测） | Implemented |

### 2.3 可观测性

| ID | 描述 | 状态 |
|----|------|------|
| NF-201 | 所有非平凡 Vulkan 调用包裹 `VK_CHECK*` 宏，错误日志含文件 + 行号 | Implemented |
| NF-202 | Debug 构建启用 Vulkan 验证层（可降级） | Implemented |
| NF-203 | 启用 ASan / UBSan 选项（互斥；TSan 单独） | Implemented |
| NF-204 | 主循环每帧上报 frame time / FPS（编辑器可视化） | Accepted |
| NF-205 | 集成 RenderDoc 调试标签（`vkCmdBeginDebugUtilsLabelEXT`） | Accepted |
| NF-206 | 支持 GPU profiling（timestamp query） | Accepted |

### 2.4 可测试性

| ID | 描述 | 状态 |
|----|------|------|
| NF-301 | core/ 全部子系统单元测试（doctest） | Implemented（75 cases / 255 assertions） |
| NF-302 | services / drivers 集成测试通过 GraphicsDevice mock | Accepted |
| NF-303 | CLI 工具具有自身单元测试（misc/scripts/） | Deferred |
| NF-304 | 关键渲染输出有 golden image 比对测试 | Deferred |

### 2.5 可维护性

| ID | 描述 | 状态 |
|----|------|------|
| NF-401 | 每个子系统/层有权威文档（`docs/dev/<area>.md`） | Implemented |
| NF-402 | `.cursor/rules/` 强制提示文档同步 | Implemented |
| NF-403 | 重大设计决策记录为 ADR | Implemented（6 条已迁入） |
| NF-404 | 命名 / 风格 / 架构约束有专门规范文档 | Implemented |
| NF-405 | 模块新增零样板（CMake 自动收集 + 注册生成） | Implemented |

### 2.6 工程化

| ID | 描述 | 状态 |
|----|------|------|
| NF-501 | 单一构建系统（CMake + Ninja） | Implemented |
| NF-502 | 工具链零额外依赖（仅 Python ≥ 3.10 标准库） | Implemented |
| NF-503 | 从仓库任何 cwd 都能运行 rover-cli | Implemented |
| NF-504 | 构建产物 `bin/<config>/` 与 `build/<config>/` 分离 | Implemented |
| NF-505 | `compile_commands.json` 自动同步到根 | Implemented |

---

## 3. 受 ADR 约束的需求

下列条目的具体落地受 ADR 约束，编辑前请先阅读对应 ADR：

| 需求 | 受约束于 ADR |
|------|-------------|
| F-004 GraphicsDevice 抽象 | [ADR-0001](adr/ADR-0001-three-layer-architecture.md) |
| F-301 GraphicsService | [ADR-0001](adr/ADR-0001-three-layer-architecture.md), [ADR-0004](adr/ADR-0004-services-vs-servers-naming.md) |
| F-401 scene 模块 | [ADR-0003](adr/ADR-0003-modules-over-fixed-scene-tree.md) |
| F-501 / F-506 编辑器 | [ADR-0005](adr/ADR-0005-cli-under-editor.md) |
| F-111 文件系统 + F-406 序列化 | [ADR-0006](adr/ADR-0006-io-split-platform-and-serialization.md) |
| NF-501 单一构建 | [ADR-0002](adr/ADR-0002-cmake-as-single-build-system.md) |

---

## 4. 维护注意

- **新增需求**：取下一个未用编号（按类别独立计数），列出描述、状态、目标版本，必要时关联 ADR
- **变更需求**：保留编号，更新描述并在「备注」加变更说明
- **废弃需求**：状态改为 `Deprecated`，备注写明替代方案
- **从需求到任务**：实现该需求时把对应条目加入 `dev/<area>.md` 的「后续工作」TODO 列表

---

*Rover Engine v0.1.0 — 需求清单与项目演进同步。*
