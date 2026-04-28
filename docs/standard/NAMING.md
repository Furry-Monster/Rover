# 命名约定

> 本规范是 Rover 引擎所有第一方代码（`core/`、`drivers/`、`platform/`、`services/`、`modules/`、`editor/`、`main/`、`tests/`）必须遵守的命名规则。vendor 第三方代码不受约束。

---

## 1. 总览速查表

| 范畴 | 约定 | 示例 |
|------|------|------|
| 命名空间 | 一律 `rover::` | `namespace rover { ... }` |
| 嵌套命名空间（可选） | `rover::math`、`rover::gfx` | `rover::math::Vector3` |
| CMake target | `rover_<area>[_<sub>]` | `rover_core`、`rover_module_scene` |
| CMake 别名 | `Rover::<Area>`（PascalCase） | `Rover::Core`、`Rover::Services` |
| 类 / 结构体 | `PascalCase` | `GraphicsService`、`SceneTree` |
| 枚举类型 | `PascalCase` | `enum class CullMode` |
| 枚举值 | `PascalCase`（使用 `enum class`） | `CullMode::Back` |
| 函数 / 方法 | `snake_case` | `register_core_types`、`begin_frame` |
| 局部变量 / 参数 | `snake_case` | `frame_dt`、`render_pass` |
| 成员变量 | `snake_case_` 末尾下划线 | `surface_`、`device_name_` |
| `static` 常量 | `kPascalCase` 或 `UPPER_SNAKE` | `kMaxFramesInFlight`、`MAX_TEXTURES` |
| 宏 | `ROVER_<AREA>_<NAME>` 全大写 | `ROVER_PLATFORM_LINUX`、`ROVER_LOG_INFO` |
| 文件名 | `snake_case.{h,cpp}` | `graphics_device.h`、`vk_swapchain.cpp` |
| 头文件守卫 | `#pragma once` | — |
| 注册函数 | `register_<scope>_types` / `unregister_<scope>_types` | `register_scene_types` |

---

## 2. 命名空间

### 规则

- 所有第一方代码顶级位于 `namespace rover { ... }` 内
- 子命名空间允许但不强制；倾向于**扁平**
- 不允许 `using namespace rover;` 出现在头文件中（cpp 文件中可酌情使用）
- 不允许 `using namespace std;`，永远显式 `std::xxx`

### 何时使用嵌套

仅在以下情况使用嵌套命名空间：

- 大量算法或工具函数集合（如 `rover::math` 下的自由函数）
- 内部实现细节，避免与公共 API 同级（如 `rover::detail::xxx`，下划线开头表示内部）

### 反例

```cpp
// ❌ 不要：在头文件 using namespace
// engine.h
using namespace rover;
using namespace std;

// ❌ 不要：过深嵌套
namespace rover::graphics::vulkan::internal::pipeline { ... }

// ✅ 推荐
namespace rover { ... }
namespace rover::math { ... }   // 算法集合可接受
namespace rover::detail { ... } // 实现细节可接受
```

---

## 3. 类型（class / struct / enum）

### 规则

- 类与结构体使用 `PascalCase`：`GraphicsService`、`Vector3`
- 即使是 POD 也用 `PascalCase`，不用 `_t` 后缀
- 一律使用 `enum class`（强类型枚举）
- 枚举值也使用 `PascalCase`（注意：不是 SCREAMING_SNAKE_CASE）

### `class` vs `struct`

- **class**：有不变量需要维护、有非平凡构造/析构、有私有成员
- **struct**：纯数据聚合，所有成员公有，无方法或仅 trivial 方法

### 反例

```cpp
// ❌ 旧 C 风格枚举
enum CullMode { CULL_MODE_NONE, CULL_MODE_BACK, CULL_MODE_FRONT };

// ❌ 错误的命名风格
enum class cull_mode { NONE, BACK, FRONT };
struct GraphicsService_Data { ... };  // 不要用下划线分类

// ✅ 推荐
enum class CullMode {
    None,
    Back,
    Front,
};

struct WindowDesc {
    const char* title  = "Rover";
    u32         width  = 1280;
    u32         height = 720;
};
```

---

## 4. 函数与变量

### 规则

- 函数 / 方法：`snake_case`（`init`、`begin_frame`、`get_swapchain_format`）
- 局部变量、参数：`snake_case`
- 成员变量：`snake_case_`（**末尾下划线**，便于和参数区分）
- `static` 局部变量同普通局部
- 不允许使用 `m_` / `g_` / `s_` 等匈牙利前缀

### 末尾下划线的目的

```cpp
class Window
{
public:
    bool init(const WindowDesc& desc);

private:
    SDL_Window* window_;   // 成员
    u32         width_;
    u32         height_;
};

bool Window::init(const WindowDesc& desc)
{
    width_  = desc.width;   // 一眼看出 width_ 是成员，desc 是参数
    height_ = desc.height;
    return true;
}
```

### Getter / Setter

- Getter：动词省略，名词或形容词命名（`width()`、`is_initialized()`、`should_close()`）
- Setter：`set_<name>`（`set_size(u32, u32)`、`set_title(const char*)`）
- 布尔 getter 用 `is_xxx` / `has_xxx` / `should_xxx`

### 反例

```cpp
// ❌ 匈牙利前缀
class Window {
    SDL_Window* m_window;
    u32 m_width;
};

// ❌ 函数 PascalCase（旧 Windows 风格）
void BeginFrame();
bool ShouldClose();

// ✅ 推荐
class Window {
    SDL_Window* window_;
    u32         width_;
};

void begin_frame();
bool should_close();
```

---

## 5. 常量

两种允许的风格：

| 风格 | 用途 | 示例 |
|------|------|------|
| `kPascalCase` | 编译期常量、类内 `static constexpr` | `kMaxFramesInFlight = 2` |
| `UPPER_SNAKE` | 全局 `constexpr` 常量、宏定义 | `EPSILON = 1e-6` |

倾向：**`kPascalCase` 用于值，`UPPER_SNAKE` 仅在确实是宏或与 C 接口对接时使用**。

```cpp
// ✅ 类内 static
class GraphicsDeviceVulkan {
    static constexpr u32 kMaxFramesInFlight = 2;
};

// ✅ 全局命名空间常量
inline constexpr f64 PI      = 3.14159265358979323846;
inline constexpr f64 EPSILON = 1e-6;

// ✅ 编译宏（罕用）
#define ROVER_DRIVER_VULKAN 1
```

---

## 6. 文件名

### 规则

- 全 `snake_case`，扩展名 `.h` / `.cpp`
- 一个文件对应一个主类，文件名 = 类名的 snake_case 形式
- 头文件守卫使用 `#pragma once`，**不**使用 `#ifndef ROVER_FOO_H_` 三联宏
- 伞形头文件（aggregator）以子系统名命名：`math.h`、`task.h`、`event.h`

### 多文件实现一个类

如果一个类太大需要拆分为多个 `.cpp`，使用统一前缀 + `_<topic>` 后缀：

```
graphics_device_vulkan.h                        声明
graphics_device_vulkan_lifecycle.cpp            init/shutdown
graphics_device_vulkan_buffer.cpp               buffer/texture/sampler
graphics_device_vulkan_pipeline.cpp             pipeline
graphics_device_vulkan_commands.cpp             命令录制
```

### 测试文件

`tests/<area>/<topic>_test.cpp`：

```
tests/core/math_test.cpp
tests/core/event_test.cpp
tests/core/allocator_test.cpp
```

---

## 7. 宏

### 规则

- 全大写 `UPPER_SNAKE`
- `ROVER_<AREA>_<NAME>` 形式，前缀 `ROVER_` 是必须的
- 仅在**真的需要宏**时使用：日志（带 `__FILE__` / `__LINE__`）、平台条件编译、外部 API 桥接
- 优先使用 `inline constexpr` 而非 `#define` 定义常量

### 既有宏分类

| 宏前缀 | 用途 | 例子 |
|--------|------|------|
| `ROVER_PLATFORM_*` | 当前平台标识 | `ROVER_PLATFORM_LINUX` |
| `ROVER_DRIVER_*` | 当前后端启用 | `ROVER_DRIVER_VULKAN` |
| `ROVER_DEBUG` / `ROVER_RELEASE` | 当前构建配置 | — |
| `ROVER_EDITOR_BUILD` | 仅编辑器构建 | — |
| `ROVER_LOG_*` / `ROVER_APP_*` | 日志宏 | `ROVER_LOG_INFO("msg {}", x)` |
| `ROVER_CLASS(...)` | 反射类注册 | `ROVER_CLASS(MyClass, Object)` |
| `VK_CHECK*` | Vulkan 错误检查 | `VK_CHECK(vkCreateBuffer(...))` |

### 反例

```cpp
// ❌ 没有 ROVER_ 前缀（污染全局命名空间）
#define MAX_BUFFERS 256
#define LOG_INFO(...) ...

// ❌ 用宏代替 const
#define PI 3.14159

// ✅ 推荐
inline constexpr u32 kMaxBuffers = 256;
inline constexpr f64 PI          = 3.14159265358979323846;

#define ROVER_LOG_INFO(...)  spdlog::info(__VA_ARGS__)   // 必须是宏（要 file/line/format string 一体）
#define VK_CHECK(expr)       /* 必须是宏（要 expr 字符串化） */
```

---

## 8. CMake target 与别名

### 规则

| 类型 | 命名 | 示例 |
|------|------|------|
| Target（实际） | `rover_<area>[_<sub>]` | `rover_core`、`rover_driver_vulkan`、`rover_module_scene` |
| 别名（公共 API） | `Rover::<Area>` | `Rover::Core`、`Rover::Drivers`、`Rover::Modules` |
| 接口库 | 同 target 规则 + `Rover::CompileFlags` 风格别名 | `rover_compile_flags` / `Rover::CompileFlags` |

### 何时设置别名

- **聚合 façade**：`Rover::Drivers`、`Rover::Platform`、`Rover::Modules`、`Rover::Editor` 给 `main/` 用
- **核心库**：`Rover::Core` 给所有上层使用
- **具体子 target**：通常不需要别名（`rover_driver_vulkan` 不必有 `Rover::DriverVulkan`）

---

## 9. 注册函数命名

注册系统是引擎的关键基础设施，命名严格统一：

| 层级 | 注册函数 |
|------|---------|
| core | `register_core_types()` / `unregister_core_types()` |
| services | `register_service_types()` / `unregister_service_types()` |
| drivers（聚合） | `register_driver_types()` / `unregister_driver_types()` |
| drivers（具体） | `register_<api>_driver()` / `unregister_<api>_driver()` |
| platform（聚合） | `register_platform_apis()` / `unregister_platform_apis()` |
| platform（具体） | `register_<os>_platform()` / `unregister_<os>_platform()` |
| modules（聚合） | `register_module_types()` / `unregister_module_types()` |
| modules（具体） | `register_<name>_types()` / `unregister_<name>_types()` |
| editor | `register_editor_types()` / `register_editor_gui()` / `register_editor_cli()` |

**强约束**：每个层 / 模块都必须暴露这些函数，且不能省略 `unregister_*`。`main/` 严格按反序调用 unregister。

---

## 10. 违规检查

| 违反 | 检查方式 |
|------|---------|
| 命名空间错误 | clang-tidy `readability-identifier-naming`（计划开启） |
| 类 / 函数命名 | code review + clang-tidy |
| 文件名 | git pre-commit hook（计划） + code review |
| 宏前缀 | grep + code review |
| 注册函数命名 | `.cursor/rules/` 检查（cursor 在编辑相关代码时提示） |

未来计划：在 `misc/scripts/rover/commands/lint.py` 中加入命名约定批量检查。

---

## 11. 参考

- [`docs/standard/CODE_STYLE.md`](CODE_STYLE.md) — clang-format / 代码风格细节
- [`docs/standard/ARCHITECTURE_RULES.md`](ARCHITECTURE_RULES.md) — 注册系统的依赖约束
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)（命名思路参考，但 Rover 不完全遵循）
- [`.clang-tidy`](../../.clang-tidy) — 当前启用的检查
- [`.clang-format`](../../.clang-format) — 格式化配置
