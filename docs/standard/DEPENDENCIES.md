# 依赖管理规范

> 本规范定义 Rover 引擎对 **第三方依赖**（vendor）和 **内部模块依赖**的管理政策。当前依赖快照见 [`docs/dev/ARCHITECTURE.md`](../dev/ARCHITECTURE.md) §4。

---

## 1. vendor 政策

### 1.1 vendor 的角色

`vendor/` 是 **第三方依赖** 的唯一容纳处：

- 所有第三方库源码（**纯源码副本**，不带 `.git/`）都在 `vendor/<libname>/`
- 由 `vendor/CMakeLists.txt` 包装为 CMake target，对外暴露 `Rover::<Lib>` 别名（或库自身原生 target）
- 第一方代码 **绝不** 直接 add_subdirectory 到第三方库源码目录之外的位置

### 1.2 添加新依赖的流程（强制）

> 架构决策：见 [ADR-0008](../product/adr/ADR-0008-vendor-manual-fetch.md)。`git submodule` / CMake `FetchContent` / `find_package` 系统库三者均**禁用**；唯一合法的引入方式是「下载源码副本到 `vendor/<libname>/` 并提交进 Rover 仓库」。

向 vendor 引入新库**必须**：

1. **写 ADR**：说明引入动机、考察过的替代方案、license 兼容性
2. **更新本文档**的"已使用依赖"清单
3. **登记到 vendor manifest**：在 `misc/scripts/vendor/manifest.toml`（待 Phase 2 落地）记录上游 URL、版本号、SHA-256，确保任何人都能复现下载。
4. **下载源码副本**到 `vendor/<libname>/`：

   | 方式 | 何时使用 | 操作 |
   | -- | -- | -- |
   | **脚本下载（首选）** | manifest 已有该库 | `python misc/scripts/vendor/fetch.py <libname>`（脚本下载 tarball、校验 SHA-256、解压、清理 `.git/`/tests/docs，生成 diff） |
   | **人工下载（兜底）** | 脚本暂未实现或库特殊 | `wget <url>` / `curl -L <url>` / 上游 release 解压；commit message 中说明来源 URL、版本号、SHA-256；同 PR 必须把该库登记到 manifest |

   完成后 `git add vendor/<libname>/` 把源码作为仓库内容提交。

5. **更新 `vendor/CMakeLists.txt`**：包装为 target，定义编译宏。优先 `add_subdirectory(...)`（库自带 CMake）或手写 INTERFACE / STATIC target（header-only 或简单源文件集合）。
6. **首选** header-only 或静态库；动态库仅在确实必要时考虑（且必须仍然由 vendor 源码本地构建产生，不允许从系统加载）。

### 1.2.1 禁止的集成方式

| 方式 | 状态 | 原因（详见 ADR-0008） |
| -- | -- | -- |
| `git submodule add` | ❌ 禁止 | 网络脆弱、`git clone` 不带 `--recursive` 必坑、与离线构建目标冲突 |
| CMake `FetchContent_Declare` / `ExternalProject_Add` | ❌ 禁止 | configure 阶段联网，破坏构建确定性，缓存不进 git 历史 |
| `find_package(<SomeSystemLib> REQUIRED)` | ❌ 禁止 | 依赖开发者本机环境，破坏 hermetic onboarding |
| 包管理器（vcpkg / Conan / xrepo） | ❌ 禁止 | 引入第二套依赖系统，违反 ADR-0002 单一构建系统 |

> **澄清**：本规范禁的是**构建期**对开发者机器的探测（`find_package` 触发的 SDK / 系统库探测）。运行期通过 `dlopen` / `LoadLibrary` 加载系统驱动（如 Vulkan ICD `libvulkan.so.1`）是图形 API 的固有机制，不在禁用范围。换言之：Vulkan、OpenGL 这类驱动入口的**头文件必须 vendor**，但**运行期由系统 ICD 加载**是可以接受的。

### 1.2.2 升级第三方库

升级 = 改 manifest 中的版本号 + SHA-256 → 重跑 fetch 脚本 → diff 进 PR：

1. 在分支上修改 `manifest.toml`
2. 跑 `python misc/scripts/vendor/fetch.py <libname> --upgrade`
3. 跑全套测试 + sanitizer
4. 更新 `docs/dev/<area>.md` 中的版本号
5. 写 commit message 标记升级原因（bug fix / 安全 / 新功能）

### 1.3 修改 vendor 源码（强制禁止）

- ❌ **永远不要直接修改** `vendor/<lib>/` 内的源码
- ✅ 如需 patch：
  - 优先在 `vendor/CMakeLists.txt` 中通过 `target_compile_definitions` / `target_compile_options` 调整
  - 必要时写 wrapper（如 `vendor/<lib>_wrapper/`）封装一层
  - 极端情况下应 fork 并在文档中说明 fork 维护策略

---

## 2. 当前 vendor 列表

所有库均为「源码副本」形式提交在仓库内，不使用 git submodule（见 [ADR-0008](../product/adr/ADR-0008-vendor-manual-fetch.md)）。

| 库 | 版本 / 来源 | License | 用途 | 集成方式 |
|----|-----------|---------|------|---------|
| **SDL3** | vendored | Zlib | 窗口 / 输入 / 计时（platform 层） | 源码副本 + `add_subdirectory` |
| **Vulkan-Headers** | vendored（待迁移）¹ | Apache 2.0 | Vulkan API 头文件 | 当前临时由 `find_package(Vulkan)` 提供 |
| **volk** | vendored | MIT | Vulkan 函数指针动态加载 | 源码副本 + `add_subdirectory` |
| **VulkanMemoryAllocator** | vendored | MIT | GPU 内存分配（VMA） | 源码副本 + `add_subdirectory`（header-only） |
| **glm** | vendored | MIT | 数学（vector / matrix / quat） | 源码副本 + INTERFACE target（header-only） |
| **EnTT** | vendored | MIT | ECS（用于 modules/scene） | 源码副本 + INTERFACE target（header-only） |
| **spdlog** | vendored | MIT | 日志（core/log 封装） | 源码副本 + `add_subdirectory` |
| **ImGui** | vendored | MIT | 编辑器 GUI（待 Phase 3） | 源码副本 + 手写 STATIC target |
| **doctest** | vendored | MIT | 单元测试框架 | 源码副本 + INTERFACE target（header-only） |

¹ 当前 `vendor/CMakeLists.txt` 仍调用 `find_package(Vulkan REQUIRED)` 获取 Vulkan 头文件，违反 §1.2.1。Phase 2 任务：把 `Vulkan-Headers` 源码 vendor 进 `vendor/Vulkan-Headers/` 并删除 `find_package` 调用（见 §2.3）。

详见：[`vendor/CMakeLists.txt`](../../vendor/CMakeLists.txt)。

### 2.1 计划引入

| 库 | 用途 | Phase | License 待审 |
|----|------|-------|-------------|
| **slang** | Slang/HLSL → SPIR-V/DXIL/MSL/WGSL 转译器（[ADR-0007](../product/adr/ADR-0007-shader-source-language.md)） | Phase 2 | Apache 2.0 |
| **tinygltf / cgltf** | GLTF 模型导入 | Phase 2 | MIT (tinygltf) / MIT (cgltf) |
| **stb_image** | 图片解码（PNG/JPG） | Phase 2 | MIT/Public Domain |
| **KTX-Software** | KTX2 纹理 | Phase 2 | Apache 2.0 |
| **Basis Universal** | KTX2 转码后端 | Phase 2 | Apache 2.0 |
| **miniaudio** 或 **OpenAL Soft** | 音频混音 | Phase 4 | MIT (miniaudio) / LGPL (OpenAL Soft) |
| **Jolt Physics** 或 **Bullet** | 物理 | Phase 4 | MIT (Jolt) / Zlib (Bullet) |
| **dxc**（可选） | HLSL → DXIL（D3D12 后端） | Phase 6 | LLVM Apache 2.0 with LLVM Exception |

最终选型由 ADR 决定。

### 2.2 License 兼容性约束

Rover 自身计划开源（具体 license 待 v1.0 决定，候选 MIT / Apache 2.0 / BSD-3-Clause）。引入依赖必须：

- ✅ MIT / BSD / Zlib / Apache 2.0 / Public Domain：直接可用
- 🟡 LGPL：可链接（动态），需注意打包
- ❌ GPL：禁止（除非有特例豁免，且记录在 ADR）

### 2.3 Vulkan 处理（澄清）

Vulkan 涉及两类入口，分别处理：

- **构建期头文件**（`vulkan/vulkan.h` 等）：必须 vendor 进 `vendor/Vulkan-Headers/`，与其他库一视同仁。**禁止** `find_package(Vulkan)`。
- **运行期加载器**（`libvulkan.so.1` / `vulkan-1.dll`）：由系统驱动栈提供，volk 通过 `dlopen` 动态加载。这是图形 API 的固有机制，不算「构建期探测系统库」，不在 ADR-0008 禁用范围内。**不**链接构建期 `Vulkan::Vulkan`，仅通过 volk 间接使用。

Phase 2 迁移完成后，`vendor/CMakeLists.txt` 中应**完全无** `find_package` 调用。

---

## 3. CMake target 依赖图

### 3.1 顶层依赖

```mermaid
graph LR
    subgraph "Layer 0: vendor"
        SDL3
        Vulkan["Rover::Vulkan<br/>(volk+VMA+Headers)"]
        glm
        spdlog
        entt
        doctest
        imgui
    end

    subgraph "Layer 1"
        Core["Rover::Core"]
    end

    subgraph "Layer 2"
        DrvVk["rover_driver_vulkan"]
        Drivers["Rover::Drivers"]
        PltLinux["rover_platform_linux"]
        Platform["Rover::Platform"]
    end

    subgraph "Layer 3"
        Services["Rover::Services"]
    end

    subgraph "Layer 4"
        Modules["Rover::Modules"]
    end

    subgraph "Layer 5"
        Editor["Rover::Editor"]
    end

    Main["rover (executable)"]
    Tests["rover_tests"]

    Core --> spdlog
    Core --> glm
    Core --> entt

    DrvVk -.PRIVATE.-> Vulkan
    DrvVk --> Core
    Drivers --> DrvVk

    PltLinux --> Core
    PltLinux --> SDL3
    Platform --> PltLinux

    Services --> Core
    Modules --> Services
    Editor --> Modules
    Editor --> imgui

    Main --> Editor
    Main --> Drivers
    Main --> Platform
    Main --> Modules

    Tests --> Modules
    Tests --> doctest
```

### 3.2 PUBLIC vs PRIVATE 规则

| 关系 | 链接类型 | 原因 |
|------|---------|------|
| `rover_core` → `spdlog` / `glm` / `entt` | PUBLIC | 模板 / inline 函数让头文件需要看到这些类型 |
| `rover_driver_vulkan` → `Rover::Vulkan` | **PRIVATE** | Vulkan 头文件不暴露给上层，否则会污染 services / modules |
| `rover_driver_vulkan` → `Rover::Core` | PUBLIC | 实现 `GraphicsDevice` 抽象需要其声明可见 |
| `rover_platform_linux` → `SDL3::SDL3` | **PRIVATE** | SDL 是平台细节，不暴露 |
| `rover_platform_linux` → `Vulkan::Headers` | **PRIVATE** | 仅 `SDL_Vulkan_CreateSurface` 内部使用 |
| `Rover::*`（聚合 façade） → 具体 target | INTERFACE | 聚合 target 自身无源码 |

**强制**：driver / platform 的第三方依赖一律 `PRIVATE`，不让 SDK 头文件透传到上层。

---

## 4. 模块依赖图（modules 内部）

```
modules/
├── scene/             基础（其他模块的世界容器）
├── animation/  ─────► scene
├── particle/   ─────► scene
├── ui/         ─────► （独立，依赖 services/graphics）
├── ai/         ─────► scene
└── serialization/─►  scene + 提供资产格式给所有模块
```

### 4.1 模块互依赖规则

- 默认模块互相 **不可依赖**（`modules/animation/` 不可 include `modules/particle/`）
- 例外：通过 `modules/<x>/CMakeLists.txt` 中显式声明依赖：
  ```cmake
  rover_add_library(rover_module_animation
      SOURCES ${_srcs}
      PUBLIC_DEPS Rover::Core Rover::Services
      PRIVATE_DEPS rover_module_scene   # 显式依赖 scene
      FOLDER "Rover/Modules"
  )
  ```
- 启用某模块时如果其依赖未启用应在 CMake 配置阶段报错

### 4.2 模块启停

| 模块 | 默认 | CMake 选项 | 依赖 |
|------|------|-----------|------|
| scene | ON | `ROVER_MODULE_SCENE` | — |
| animation | ON | `ROVER_MODULE_ANIMATION` | scene（计划） |
| particle | ON | `ROVER_MODULE_PARTICLE` | scene（计划） |
| ui | ON | `ROVER_MODULE_UI` | services/graphics |
| ai | ON | `ROVER_MODULE_AI` | scene |
| serialization | ON | `ROVER_MODULE_SERIALIZATION` | — |

---

## 5. 服务依赖

服务层（`services/`）之间的互访限制：

| 服务 | 可调用 |
|------|--------|
| GraphicsService | core/graphics 抽象 |
| PhysicsService | core 抽象 |
| AudioService | core 抽象 |
| NetworkService | core 抽象 |

**强制**：服务之间**不直接** include 对方头文件。如有跨服务通信需求，通过 `core/event/EventBus` 异步通信，或在 `main/` 中显式装配依赖（少数场景）。

---

## 6. 引入第三方库的决策矩阵

新库引入前回答以下问题：

1. **YAGNI 检查**：当前 phase 真的需要它吗？还是 phase+1 才用？
2. **替代方案**：标准库 / 已有依赖 / 自己写一小段 能解决吗？
3. **可移植性**：在所有目标平台（Linux/Windows/Mac/Android/iOS/Web）上可用吗？
4. **维护活跃度**：上游最近一年有提交吗？issue 响应及时吗？
5. **license 兼容**：是否落入 §2.2 的允许 license？
6. **二进制体积**：会增加多少二进制 / 编译时间？
7. **替换成本**：未来想换掉时，被多少代码 include？
8. **vendor 体积**：源码副本会让 Rover 仓库增加多少？是否需要 git-lfs？
9. **下载源稳定性**：上游是否提供稳定 release tarball 与 SHA-256？

任意一项答 "No / 不确定"，至少在 ADR 中明确说明。

---

## 7. 反例

```
❌ 在 services/graphics/CMakeLists.txt 中：
target_link_libraries(rover_services PUBLIC Rover::Vulkan)
   → 把 vulkan.h 拽到所有上层，违反抽象层

✅ 应该在 drivers/vulkan/CMakeLists.txt 中：
target_link_libraries(rover_driver_vulkan PRIVATE Rover::Vulkan)

❌ 直接 #include <vulkan/vulkan.h> 在 modules/scene/scene_tree.h
   → 模块层不应见 Vulkan，应通过 GraphicsService 抽象间接使用

❌ 引入 boost 单单为了 boost::optional
   → C++20 已有 std::optional，无需引入庞大的 boost

❌ 修改 vendor/SDL/src/video/x11/SDL_x11events.c 来"修复"一个 bug
   → 应当上游 PR；本地 patch 必须有 ADR + 文档说明

❌ 在 core/CMakeLists.txt 中 target_link_libraries(rover_core PUBLIC SDL3::SDL3)
   → core 不应依赖 SDL（SDL 是平台细节）

❌ 用 git submodule 引入新库
   → git submodule add https://github.com/foo/bar vendor/bar
   → 违反 ADR-0008；应当下载 release tarball 解压到 vendor/bar/ 并 git add 全部源码

❌ 用 CMake FetchContent 拉取依赖
   → FetchContent_Declare(foo URL https://...)
   → 违反 ADR-0008；configure 阶段联网破坏构建确定性

❌ 用 find_package 探测系统库（Vulkan 入口的运行期加载除外，且仍不允许构建期链接）
   → find_package(SDL3 REQUIRED) → 拒绝，必须 vendor 源码
   → find_package(spdlog REQUIRED) → 拒绝，必须 vendor 源码

❌ 引入 vcpkg / Conan / xrepo 来管理第三方库
   → 违反 ADR-0002（单一 CMake 构建系统）与 ADR-0008（手动下载策略）
```

---

## 8. CI 检查（计划）

未来在 `misc/scripts/rover/commands/lint.py` 中加入：

- 检查 driver / platform target 是否所有第三方 deps 都是 PRIVATE
- 检查 services / modules 是否 include 了 driver / platform 头文件
- 检查 vendor/ 是否有未提交的本地修改
- **检查根目录不存在 `.gitmodules`**（强制 ADR-0008：禁用 submodule）
- **检查仓库内 CMake 文件不出现 `FetchContent_Declare` / `ExternalProject_Add`**（强制 ADR-0008）
- **检查 `vendor/CMakeLists.txt` 不出现 `find_package` 调用**（Vulkan 迁移完成后；过渡期允许 Vulkan 单点例外）
- **检查每个 `vendor/<lib>/` 目录在 `manifest.toml` 中有对应条目**（强制 ADR-0008 §1.2 step 3）
- **检查 vendor 源码副本不携带 `.git/`**（强制 ADR-0008 §1.2 step 4）

---

## 9. 参考

- [`docs/dev/ARCHITECTURE.md`](../dev/ARCHITECTURE.md) §4 CMake 目标依赖图
- [`docs/standard/ARCHITECTURE_RULES.md`](ARCHITECTURE_RULES.md) §6 CMake target 约束
- [`vendor/CMakeLists.txt`](../../vendor/CMakeLists.txt)
- [ADR-0001 三层倒置](../product/adr/ADR-0001-three-layer-architecture.md)
- [ADR-0002 单一 CMake 构建](../product/adr/ADR-0002-cmake-as-single-build-system.md)
- [ADR-0007 Shader 源语言选 Slang/HLSL + 转译器](../product/adr/ADR-0007-shader-source-language.md)
- [ADR-0008 vendor 手动下载（禁用 submodule / FetchContent / find_package）](../product/adr/ADR-0008-vendor-manual-fetch.md)
