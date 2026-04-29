# 架构规则

> 本规范定义 Rover 引擎**必须遵守**的层依赖与 include 约束。这是 [ADR-0001 三层倒置架构](../product/adr/ADR-0001-three-layer-architecture.md) 在编码层面的强制条款。当前实现快照见 [`docs/dev/ARCHITECTURE.md`](../dev/ARCHITECTURE.md)。

---

## 1. 分层模型（强制）

```
Layer 5  editor/                       gui + cli 编辑器接口
Layer 4  modules/                      可插拔功能模块
Layer 3  services/                     重型单例服务
Layer 2  drivers/  +  platform/        实现 core 抽象 / 暴露 OS
Layer 1  core/                         类型 / 数学 / 事件 / 任务 / 抽象接口
Layer 0  vendor/                       第三方依赖
```

**层级编号 = 依赖方向**：编号越低依赖越少。任意层只能依赖 **编号更低** 的层（vendor 是 0，不依赖 Rover 任何层）。

---

## 2. 依赖矩阵（强制）

下表中 ✅ = 允许 include，❌ = 禁止 include，⚠️ = 仅运行时通过抽象指针。

| 来源 \ 目标 | core | drivers | platform | services | modules | editor | main | vendor |
|------------|:----:|:-------:|:--------:|:--------:|:-------:|:------:|:----:|:------:|
| **core/**       | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |
| **drivers/<x>/** | ✅ | ❌（其他 driver） | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |
| **platform/<os>/** | ✅ | ❌ | ❌（其他 OS） | ❌ | ❌ | ❌ | ❌ | ✅（SDL3 + Vulkan::Headers） |
| **services/**   | ✅ | ⚠️ 仅通过 core 抽象 | ⚠️ 仅通过 platform façade | ✅ | ❌ | ❌ | ❌ | ✅（与 core 一致） |
| **modules/<x>/** | ✅ | ❌ | ❌ | ✅ | ❌（其他 module，除非显式依赖） | ❌ | ❌ | ✅ |
| **editor/**     | ✅ | ⚠️ | ⚠️ | ✅ | ✅ | ✅ | ❌ | ✅ |
| **main/**       | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | — | ✅ |

### 2.1 关键约束（违反必拒）

1. **services 不可 include 任何 driver 头文件**：要调用 GPU，只能通过 `core/graphics/graphics_device.h` 抽象
2. **modules 不可 include 任何 driver 或 platform 头文件**：要使用平台/GPU 能力，必须经过 services 或 core
3. **同层互不依赖**：`drivers/vulkan/` 不可 include `drivers/d3d12/`，`platform/linux/` 不可 include `platform/windows/`
4. **core 是最底层**：`core/` 不能 include 除 vendor 外任何其他第一方代码
5. **main 是唯一组合根**：只有 `main/` 可以同时 include 抽象与具体（如 `get_vulkan_device()` 与 `GraphicsDevice*`）

### 2.2 为什么 services 不能 include driver？

```cpp
// ❌ 错误
// services/graphics/graphics_service.cpp
#include "drivers/vulkan/graphics_device_vulkan.h"  // 把 vulkan.h 拽到 services 层
                                                      // → 间接 #include <X11/Xlib.h>
                                                      // → #define None 0 与 CullMode::None 冲突
                                                      // → 无法编译 D3D12 后端的 services

// ✅ 正确
// services/graphics/graphics_service.cpp
#include "core/graphics/graphics_device.h"          // 仅依赖抽象
GraphicsDevice* device_;                              // 由 main/ 注入
```

---

## 3. include 路径规则

**强制**：所有第一方 include 使用 **仓库根相对路径**：

```cpp
// ✅
#include "core/math/vector3.h"
#include "services/graphics/graphics_service.h"

// ❌
#include "../../math/vector3.h"   // 相对路径
#include "vector3.h"               // 平铺
```

`rover_add_library()` 自动把 `ROVER_ROOT_DIR` 加入 `target_include_directories(... PUBLIC)`，无需在 CMakeLists.txt 中重复声明。

---

## 4. 抽象接口契约

### 4.1 抽象在 core/ 声明，drivers / platform 实现

| 抽象接口（core/） | 实现位置 | 注入到 |
|-----------------|---------|--------|
| `GraphicsDevice` (`core/graphics/graphics_device.h`) | `drivers/<api>/` | `services/graphics/` 通过 `main/` 注入 |
| `WindowSystem` (`core/graphics/window_system.h`) | `platform/<os>/window.{h,cpp}` | `drivers/<api>/` 通过 `init(WindowSystem&)` 注入 |
| `FileSystem` (`core/os/`，未来) | `platform/<os>/file.cpp` | `services/` 与 `modules/` |

### 4.2 接口扩展规则

向 `GraphicsDevice` 等抽象接口 **新增**纯虚方法时：

1. 必须先在 `docs/product/adr/` 写新 ADR 说明动机
2. 同时更新 `docs/dev/CORE.md` 与 `docs/standard/ARCHITECTURE_RULES.md`
3. 同步在所有现有实现（`drivers/vulkan/` 等）中实现新方法
4. 提供合理的默认行为或明确「未实现时调用是 UB / Fatal」契约

### 4.3 接口移除规则

移除已有的纯虚方法：

1. 必须先 ADR 说明替代方案
2. 标 `[[deprecated]]` 至少一个版本
3. 全引擎确认无调用方后再移除

---

## 5. 注册系统约束

### 5.1 注册函数签名（强制）

所有层 / 模块 / 驱动暴露的注册函数：

```cpp
namespace rover {
void register_<scope>_types();      // 首次启动调用
void unregister_<scope>_types();    // 关闭时反序调用，幂等
}
```

`<scope>` 命名见 [`NAMING.md` §9](NAMING.md#9-注册函数命名)。

### 5.2 注册顺序（强制）

`main/` 必须按以下顺序调用：

```
1. register_core_types()        （日志、ClassDB 等基础设施）
2. register_service_types()      （服务单例创建，持有抽象指针）
3. register_driver_types()       （具体驱动注册到核心）
4. register_platform_apis()      （OS 服务挂入）
5. register_module_types()       （所有启用模块）
6. register_editor_types()       （仅 ROVER_EDITOR_BUILD）
```

关闭时严格反序：`unregister_editor_types()` → `unregister_module_types()` → ... → `unregister_core_types()`。

### 5.3 关键不变量

- **服务先于驱动注册**：服务持有抽象指针，驱动注册时把自己绑定到服务
- **driver 注册不分配 GPU 资源**：那是 `init()` 的工作；register 阶段只创建实例
- **register 函数不能依赖未注册的层**：例如 `register_module_types()` 不能假设 services 是否已配置完成

---

## 6. CMake target 约束

### 6.1 命名

见 [`NAMING.md` §8](NAMING.md#8-cmake-target-与别名)。

### 6.2 依赖暴露

| 依赖类型 | 何时使用 | 例子 |
|---------|---------|-----|
| `PUBLIC` | 头文件中 include 的 target | `target_link_libraries(rover_core PUBLIC spdlog entt)` |
| `PRIVATE` | 仅 `.cpp` 中使用 | `target_link_libraries(rover_driver_vulkan PRIVATE Rover::Vulkan)` |
| `INTERFACE` | 自身无源码（接口库） | `target_link_libraries(Rover::CompileFlags INTERFACE ...)` |

**强约束**：永远不使用 `PUBLIC` 把具体 driver 暴露给上层。`drivers/vulkan/` 链接 `Rover::Vulkan` 必须是 `PRIVATE`，否则 `services/` / `modules/` 会被迫 include vulkan.h。

### 6.3 聚合 façade

四个 `Rover::Drivers`、`Rover::Platform`、`Rover::Modules`、`Rover::Editor` 是 **聚合 façade**，仅给 `main/` 提供稳定链接名。它们 `target_link_libraries(... INTERFACE rover_driver_vulkan rover_module_scene ...)` 透传具体 target，自身无源码。

---

## 7. 平台 / 后端条件编译

### 7.1 编译期宏

| 宏 | 何时定义 | 由哪个 target 提供 |
|----|---------|-------------------|
| `ROVER_PLATFORM_LINUX`  / `_WINDOWS` / `_MAC` / `_ANDROID` / `_IOS` / `_WEB` | 当前平台 | `Rover::CompileFlags` |
| `ROVER_DEBUG` / `ROVER_RELEASE` | 当前 build type | `Rover::CompileFlags` |
| `ROVER_DRIVER_VULKAN` / `_D3D12` / `_METAL` 等 | 该 driver 启用 | 对应 `rover_driver_*`（PUBLIC） |
| `ROVER_EDITOR_BUILD` | 编辑器构建 | `rover` 可执行（编辑器构建时定义） |

### 7.2 条件编译规则

- **优先使用编译选项 `option(ROVER_*)`** 控制是否构建某个 target
- **次选 `#ifdef`** 用于平台分支（platform/register_platform_apis.cpp 必然有 `#if defined(ROVER_PLATFORM_LINUX) ... #elif ...`）
- **避免** 在业务代码（services / modules）中写 `#ifdef ROVER_DRIVER_VULKAN`

```cpp
// ✅ 在 platform/register_platform_apis.cpp 中
#if defined(ROVER_PLATFORM_LINUX)
    register_linux_platform();
#elif defined(ROVER_PLATFORM_WINDOWS)
    register_windows_platform();
#endif

// ❌ 在 services/graphics/graphics_service.cpp 中
#ifdef ROVER_DRIVER_VULKAN
    create_vulkan_specific_resource();   // 违反抽象层
#endif
```

---

## 8. 严禁项清单（违反 = 拒绝合入）

1. ❌ `services/` / `modules/` / `editor/` 中出现 `#include "drivers/.../*.h"`
2. ❌ `services/` / `modules/` 中出现 `#include "platform/.../*.h"`
3. ❌ 任何层 `using namespace rover;` 出现在头文件中
4. ❌ 注册函数命名不符合 [`NAMING.md` §9](NAMING.md#9-注册函数命名) 的规则
5. ❌ 注册顺序错乱（如 `register_module_types()` 在 `register_core_types()` 之前）
6. ❌ 抽象接口新增纯虚方法但未在 ADR 中说明
7. ❌ CMake target 用 `PUBLIC` 暴露 driver-specific 依赖到上层
8. ❌ 跨同层互依赖（`drivers/vulkan/` include `drivers/d3d12/`）
9. ❌ vendor 目录被修改（即使是为了"小修一下"也禁止；改动应通过 wrapper 在 vendor/CMakeLists.txt 实现）
10. ❌ 引入第二套构建系统（违反 [ADR-0002](../product/adr/ADR-0002-cmake-as-single-build-system.md)）

---

## 9. 检查机制

| 违反 | 检查方式 |
|------|---------|
| include 跨层 | code review + 计划：`rover lint` 静态扫描 |
| 注册顺序 | `main/main.cpp` 集中检查；建议添加 runtime assert |
| CMake 依赖暴露 | 编辑 CMakeLists.txt 时由 `architecture-doc-sync` cursor rule 提示 |
| 抽象接口扩展 | code review；`drivers-doc-sync` rule 强制更新 `docs/dev/DRIVERS.md` |

---

## 10. 加层 / 改层 流程

如果需要新增 / 修改 / 删除 Layer，必须：

1. 写 ADR 说明动机与影响
2. 更新本文档（`ARCHITECTURE_RULES.md`）的层级模型
3. 更新 `docs/dev/ARCHITECTURE.md` 的当前实现快照
4. 更新所有依赖矩阵
5. 全引擎扫描违反新规则的现有代码并修复

---

## 11. 参考

- [`docs/dev/ARCHITECTURE.md`](../dev/ARCHITECTURE.md) — 当前实现的层级图、CMake target 列表
- [`docs/standard/NAMING.md`](NAMING.md) — 命名约定
- [`docs/standard/CODE_STYLE.md`](CODE_STYLE.md) — 代码风格
- [`docs/standard/DEPENDENCIES.md`](DEPENDENCIES.md) — vendor 政策与模块依赖
- [ADR-0001 三层倒置架构](../product/adr/ADR-0001-three-layer-architecture.md)
