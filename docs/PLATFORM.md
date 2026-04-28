# Rover 引擎 Platform 子系统文档

> **权威文档** — 对 `platform/` 的任何修改都 **必须** 同步更新本文档。修改前请先阅读本文档了解现有设计。

---

## 目录

1. [概述](#1-概述)
2. [文件清单](#2-文件清单)
3. [Window — 窗口与 Vulkan Surface](#3-window--窗口与-vulkan-surface)
4. [输入事件 — `input_events.h`](#4-输入事件--input_eventsh)
5. [EventPump — SDL → EventBus 翻译器](#5-eventpump--sdl--eventbus-翻译器)
6. [TimeSource — 高精度计时](#6-timesource--高精度计时)
7. [LinuxPlatform — 平台聚合外观](#7-linuxplatform--平台聚合外观)
8. [生命周期与注册](#8-生命周期与注册)
9. [依赖关系](#9-依赖关系)
10. [线程安全性](#10-线程安全性)
11. [扩展到其他平台](#11-扩展到其他平台)
12. [后续工作](#12-后续工作)

---

## 1. 概述

`platform/` 是 Rover 引擎的 **Layer 2**，提供 OS 级入口、窗口、文件系统、时间源和事件泵。当前仅 `platform/linux/` 实现完整；其他平台（Windows、macOS、Android、iOS、Web）保持桩文件，按相同协议实现即可。

Linux 平台层基于 **SDL3** 抹平窗口与输入差异，并通过 `core/graphics/window_system.h` 中的抽象接口与 `drivers/vulkan/` 协作创建 Vulkan surface。

| 组件 | 文件 | 职责 |
|------|------|------|
| `Window` | `window.{h,cpp}` | SDL3 窗口、`WindowSystem` 实现、Vulkan 集成 |
| `EventPump` | `event_pump.{h,cpp}` | SDL 事件 → 引擎事件总线翻译 |
| `TimeSource` | `time_source.{h,cpp}` | 高精度计时器、delta time |
| `LinuxPlatform` | `linux_platform.{h,cpp}` | 进程级单例 façade，拥有上述三个对象 |
| 输入事件 | `input_events.h` | `KeyEvent`、`MouseButtonEvent` 等 POD 载荷 |

CMake target：`rover_platform_linux`（具体）+ `rover_platform`（聚合 façade，别名 `Rover::Platform`），类型 STATIC，依赖 `Rover::Core`、`SDL3::SDL3`、`Vulkan::Headers`、`rover_platform_libs`。

---

## 2. 文件清单

```
platform/
├── register_platform_apis.{h,cpp}   平台层注册聚合（条件编译选当前 OS）
├── CMakeLists.txt                    平台聚合构建
│
└── linux/
    ├── window.{h,cpp}                Window 类
    ├── event_pump.{h,cpp}            EventPump 类
    ├── time_source.{h,cpp}           TimeSource 类
    ├── linux_platform.{h,cpp}        LinuxPlatform 单例
    ├── input_events.h                输入事件 POD + KeyCode/MouseButton 枚举
    ├── register_types.{h,cpp}        register_linux_platform 入口
    └── CMakeLists.txt                rover_platform_linux 静态库
```

其他平台（`windows/`、`mac/`、`android/`、`ios/`、`web/`）保留 `register_types.{h,cpp}` 与 `CMakeLists.txt` 占位，未实现。

---

## 3. Window — 窗口与 Vulkan Surface

### 3.1 创建参数 — `WindowDesc`

```cpp
struct WindowDesc {
    const char* title     = "Rover";
    u32         width     = 1280;
    u32         height    = 720;
    bool        resizable = true;
    bool        vulkan    = true;   // 等价于 SDL_WINDOW_VULKAN
};
```

### 3.2 类签名

```cpp
class Window : public WindowSystem {
public:
    Window();
    ~Window() override;

    bool init(const WindowDesc& desc);
    void shutdown();

    // WindowSystem 实现
    bool create_vulkan_surface(void* instance, void** surface_out) override;
    void get_vulkan_required_extensions(std::vector<const char*>& out) override;
    [[nodiscard]] u32  get_width()    const override;
    [[nodiscard]] u32  get_height()   const override;
    [[nodiscard]] bool should_close() const override;

    // 平台内部（EventPump 调用）
    [[nodiscard]] SDL_Window* native() const;
    void mark_should_close();
    void set_size(u32 w, u32 h);
};
```

### 3.3 关键实现细节

- **SDL3 创建标志**：`SDL_CreateWindow(title, w, h, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE)`。注意 SDL3 的 `SDL_CreateWindow` 不再有 x/y 参数（与 SDL2 不同）。
- **Vulkan surface 创建**：`SDL_Vulkan_CreateSurface(window_, instance, allocator, surface)`，SDL3 签名增加了 `allocator` 参数（传 `nullptr`）。
- **Vulkan 扩展查询**：`SDL_Vulkan_GetInstanceExtensions(&count)` 直接返回 `const char* const*`，无需中间缓冲区（与 SDL2 不同）。
- **不可复制**：`Window` 持有唯一 `SDL_Window*` 资源，复制构造与赋值已 `delete`。
- **析构自动 shutdown**：`~Window()` 调用 `shutdown()` 释放窗口。

### 3.4 与 Vulkan 驱动的契约

- `Window` 实现 `WindowSystem` 接口，向驱动暴露不透明 `void*` 句柄
- 驱动通过 `create_vulkan_surface()` 获得 `VkSurfaceKHR`，无需 include SDL 头文件
- `get_vulkan_required_extensions()` 在 `instance` 创建前查询所需扩展（如 `VK_KHR_surface`、`VK_KHR_xlib_surface` 等）

### 3.5 与 EventPump 的契约

- `EventPump` 通过 `native()` 拿到 `SDL_Window*`（仅用于事件路由判断）
- `EventPump::poll()` 处理 `SDL_EVENT_QUIT` / `SDL_EVENT_WINDOW_RESIZED` 时调用 `mark_should_close()` / `set_size()` 改变 `Window` 状态
- 主循环每帧检查 `window.should_close()` 决定是否退出

---

## 4. 输入事件 — `input_events.h`

### 4.1 KeyCode 枚举

平台无关的按键码，独立于 `SDL_Scancode` 数值。已映射的键：

| 类别 | 范围 |
|------|------|
| 字母 | `A` - `Z` |
| 数字 | `Num0` - `Num9` |
| 功能键 | `F1` - `F12` |
| 方向键 | `Left`, `Right`, `Up`, `Down` |
| 控制键 | `Space`, `Enter`, `Escape`, `Tab`, `Backspace`, `Insert`, `Delete`, `Home`, `End`, `PageUp`, `PageDown` |
| 修饰键 | `LeftShift`/`RightShift`, `LeftCtrl`/`RightCtrl`, `LeftAlt`/`RightAlt`, `LeftSuper`/`RightSuper` |
| 标点 | `Minus`, `Equals`, `LeftBracket`, `RightBracket`, `Backslash`, `Semicolon`, `Apostrophe`, `Grave`, `Comma`, `Period`, `Slash` |
| 未识别 | `Unknown` (= 0) |

未映射的物理键返回 `KeyCode::Unknown`。

### 4.2 修饰键位标志

```cpp
inline constexpr u32 KeyMod_Shift = 1u << 0;
inline constexpr u32 KeyMod_Ctrl  = 1u << 1;
inline constexpr u32 KeyMod_Alt   = 1u << 2;
inline constexpr u32 KeyMod_Super = 1u << 3;
```

`KeyEvent::modifiers` 是这些位的组合（左右键合并）。

### 4.3 鼠标按钮

```cpp
enum class MouseButton : u8 {
    Left   = 0,
    Right  = 1,
    Middle = 2,
    X1     = 3,
    X2     = 4,
};
```

### 4.4 事件 POD 载荷

| 类型 | 字段 | 触发时机 |
|------|------|---------|
| `KeyEvent` | `key`, `pressed`, `repeat`, `modifiers` | 键盘按下/松开 |
| `MouseButtonEvent` | `button`, `pressed`, `x`, `y` | 鼠标按键 |
| `MouseMoveEvent` | `x`, `y`, `dx`, `dy` | 鼠标移动 |
| `MouseWheelEvent` | `dx`, `dy` | 滚轮滚动 |
| `WindowResizeEvent` | `width`, `height` | 窗口尺寸变化 |
| `WindowCloseEvent` | (空) | 用户请求关闭窗口 |
| `WindowFocusEvent` | `focused` | 焦点获得/失去 |

所有事件类型都是简单 POD 结构体，通过 `EventBus::publish<EventType>(event)` 发布。

---

## 5. EventPump — SDL → EventBus 翻译器

### 5.1 类签名

```cpp
class EventPump {
public:
    EventPump(EventBus& bus, Window& window);
    void poll();   // 调用每帧一次

private:
    EventBus& bus_;
    Window&   window_;
    f32       last_mouse_x_, last_mouse_y_;
    bool      have_last_mouse_;

    [[nodiscard]] KeyCode translate_scancode(int scancode) const;
    [[nodiscard]] u32     translate_modifiers(u16 sdl_mod) const;
};
```

### 5.2 SDL 事件 → 引擎事件映射

| SDL 事件 | 引擎事件 | 副作用 |
|----------|---------|--------|
| `SDL_EVENT_QUIT` | `WindowCloseEvent` | `window_.mark_should_close()` |
| `SDL_EVENT_WINDOW_RESIZED` | `WindowResizeEvent` | `window_.set_size(w, h)` |
| `SDL_EVENT_WINDOW_FOCUS_GAINED/LOST` | `WindowFocusEvent` | — |
| `SDL_EVENT_KEY_DOWN/UP` | `KeyEvent` | — |
| `SDL_EVENT_MOUSE_BUTTON_DOWN/UP` | `MouseButtonEvent` | — |
| `SDL_EVENT_MOUSE_MOTION` | `MouseMoveEvent` | 维护 `last_mouse_*_` 用于计算 dx/dy |
| `SDL_EVENT_MOUSE_WHEEL` | `MouseWheelEvent` | — |
| 其他 | 忽略 | — |

### 5.3 鼠标 dx/dy 策略

- 第一帧使用 SDL 自身提供的 `xrel`/`yrel`（SDL 内部计算的相对位移）
- 后续帧使用上次记录位置与当前位置之差
- 这避免了首帧 dx/dy 异常大的问题

### 5.4 SDL3 注意事项

- 事件类型枚举使用 `SDL_EVENT_*` 前缀（不是 SDL2 的 `SDL_KEYDOWN` 等）
- 键盘事件使用 `e.key.scancode`（不是 `e.key.keysym.scancode`）
- 鼠标按钮事件 `e.button.x/y` 是 `float`（不是 `int`）
- `SDL_PollEvent` 返回 `bool`（SDL2 是 `int`）

---

## 6. TimeSource — 高精度计时

### 6.1 类签名

```cpp
class TimeSource {
public:
    TimeSource();           // 在 SDL_Init 之后构造
    void tick();            // 每帧调用一次

    [[nodiscard]] f64 elapsed_seconds() const;   // 自构造起的总秒数
    [[nodiscard]] f64 delta_seconds()   const;   // 上一帧持续时间
    [[nodiscard]] u64 elapsed_micros()  const;   // 自构造起的总微秒数
    [[nodiscard]] u64 frame_count()     const;
};
```

### 6.2 实现细节

- 基于 `SDL_GetPerformanceCounter()` / `SDL_GetPerformanceFrequency()`，提供纳秒级精度
- `elapsed_seconds()` / `elapsed_micros()` 实时调用 `SDL_GetPerformanceCounter()`，反映最新时间（不依赖 `tick()`）
- `delta_seconds()` 仅在 `tick()` 被调用时刷新
- `elapsed_micros()` 使用 `ticks * 1e6 / frequency` 顺序避免精度损失，u64 范围下可运行约数百年才溢出

---

## 7. LinuxPlatform — 平台聚合外观

### 7.1 类签名

```cpp
class LinuxPlatform {
public:
    [[nodiscard]] static LinuxPlatform& get();    // Meyer's singleton

    bool init();                                   // SDL_Init + 创建 window/eventpump/timesource
    void shutdown();                               // 反序销毁

    [[nodiscard]] Window&     window();
    [[nodiscard]] EventPump&  event_pump();
    [[nodiscard]] TimeSource& time();
    [[nodiscard]] EventBus&   events();
    [[nodiscard]] bool        initialized() const;

private:
    EventBus                    event_bus_;
    std::unique_ptr<Window>     window_;
    std::unique_ptr<EventPump>  event_pump_;
    std::unique_ptr<TimeSource> time_source_;
    bool                        initialized_ = false;
};
```

### 7.2 init / shutdown

`init()` 顺序：
1. `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)` — 初始化 SDL3 视频与事件子系统
2. 创建 `Window` 并以默认 `WindowDesc{}` 初始化
3. 创建 `EventPump`（绑定 `event_bus_` 与 `window_`）
4. 创建 `TimeSource`
5. 设置 `initialized_ = true`

任一步骤失败都会回滚已创建的对象并 `SDL_Quit()`。

`shutdown()` 反序销毁：
1. `time_source_.reset()`
2. `event_pump_.reset()`
3. `window_->shutdown()` + `window_.reset()`
4. `SDL_Quit()`

### 7.3 单例语义

- Meyer's singleton：`static LinuxPlatform instance` 在 `get()` 内部
- 复制/赋值已 `delete`
- 线程安全：仅在 main 线程调用 `init()` / `shutdown()`；`get()` 本身在 C++11 后线程安全

---

## 8. 生命周期与注册

### 8.1 注册接口

`platform/linux/register_types.h`：
```cpp
void register_linux_platform();
void unregister_linux_platform();
```

`platform/register_platform_apis.cpp` 通过 `#if defined(ROVER_PLATFORM_LINUX)` 等宏选择具体平台的入口。

### 8.2 在 main 中的位置

```cpp
register_core_types();           // 1. 日志、ClassDB
register_service_types();        // 2. 服务（当前空）
register_driver_types();         // 3. 创建 GraphicsDeviceVulkan 实例（init 推迟）
register_platform_apis();        // 4. 创建窗口 + EventPump + TimeSource
register_module_types();         // 5. 模块

// 主循环中：
//   device->init(window)   ← 此时 window 已创建，driver 才能 init

run_main_loop();

// 反序 unregister
unregister_platform_apis();      // 关闭 SDL，销毁窗口
```

### 8.3 与 Vulkan 驱动的协作流程

```
register_driver_types()       // 创建 GraphicsDeviceVulkan*（未 init）
register_platform_apis()      // 创建 Window
                              //
run_main_loop():              //
    device->init(window)      // 通过 WindowSystem& 拿到所需 extension + surface
    create render resources
    while (!window.should_close()):
        event_pump.poll()     // 翻译 SDL 事件，可能修改 window
        time.tick()           //
        device->begin_frame() //
        ...                   //
        device->present()     //
    device->shutdown()        //
```

---

## 9. 依赖关系

### 9.1 外部依赖

| 来源 | 用途 |
|------|------|
| `SDL3::SDL3` | 窗口、事件、计时、Vulkan surface |
| `Vulkan::Headers` | `VkInstance` / `VkSurfaceKHR` 类型，仅用于 `SDL_Vulkan_CreateSurface` 调用 |
| `Rover::Core` | 日志、事件总线、类型别名 |
| `rover_platform_libs` | 系统库（`dl`、`pthread`） |

### 9.2 内部依赖图

```
input_events.h
    ↓
window.h ← WindowSystem (core/graphics/)
    ↓
event_pump.h
    ↓
linux_platform.h
    ↓
register_types.cpp
```

### 9.3 严禁依赖

`platform/linux/` **绝不可以** include：
- `drivers/*`（驱动只通过抽象接口 `WindowSystem` 调用回平台）
- `services/*`
- `modules/*`
- `editor/*`

---

## 10. 线程安全性

| 类 | 线程安全 | 备注 |
|----|---------|------|
| `Window` | **否** | SDL_Window 必须在创建它的线程使用；通常仅主线程调用 |
| `EventPump::poll()` | **否** | `SDL_PollEvent` 必须在主线程调用 |
| `TimeSource` | 部分 | `tick()` 仅主线程调用；`elapsed_*()` 读取无锁，多线程读取本质安全（但有数据竞争 UB 风险） |
| `LinuxPlatform::get()` | 是 | C++11 静态局部变量初始化保证 |
| `LinuxPlatform::init()/shutdown()` | **否** | 只在主线程调用一次 |
| 输入事件 POD | 是 | 值类型，无共享状态 |

**通用规则**：所有平台对象都假设在主线程使用。多线程并发由 `core/task/` 的 JobSystem 管理。

---

## 11. 扩展到其他平台

新平台 `<os>` 的实现步骤：

1. 创建 `platform/<os>/` 目录
2. 实现以下接口（与 Linux 完全平行）：
   - `window.{h,cpp}` 中的 `Window` 类继承 `WindowSystem`
   - `event_pump.{h,cpp}` 中的 `EventPump` 调用平台原生事件 API
   - `time_source.{h,cpp}` 中的 `TimeSource`
   - `<os>_platform.{h,cpp}` 中的单例 façade
   - `register_types.{h,cpp}` 中 `register_<os>_platform()` / `unregister_<os>_platform()`
3. 编写 `platform/<os>/CMakeLists.txt`，链接 `Rover::Core` + 平台原生库（如 Win32：`user32 ws2_32`；macOS：Cocoa frameworks）
4. 在 `misc/cmake/RoverOptions.cmake` 的 `set_property(... STRINGS ...)` 中加入 `<os>`
5. 在 `misc/cmake/RoverCompiler.cmake` 添加 `ROVER_PLATFORM_<OS>` 宏定义
6. 在 `platform/register_platform_apis.cpp` 添加 `#elif defined(ROVER_PLATFORM_<OS>)` 分支

**复用 SDL3**：如果新平台同样使用 SDL3，可以从 Linux 实现复制，仅修改 CMake 平台库依赖。理论上 Windows 和 macOS 实现可以与 Linux 几乎相同，只需调整窗口标志（如 macOS 的 retina 支持）。

---

## 12. 后续工作

### 已完成

- [x] SDL3 窗口创建（标题、尺寸、resizable、Vulkan）
- [x] Vulkan surface 创建与扩展查询（通过 `WindowSystem` 接口）
- [x] 键盘 + 鼠标事件翻译为引擎事件
- [x] 高精度 delta time
- [x] LinuxPlatform 单例 + 注册集成
- [x] 测试通过：窗口启动 + 三角形渲染主循环

### 待做

- [ ] 文件系统抽象（`core/io/` 层 + 平台实现）
- [ ] 游戏控制器输入（`SDL_GAMEPAD_*` 事件 → 引擎事件）
- [ ] 触摸输入（`SDL_EVENT_FINGER_*`）
- [ ] 剪贴板 / 拖放支持
- [ ] 多窗口支持（当前 `LinuxPlatform` 只持有一个窗口）
- [ ] 全屏 / borderless 切换
- [ ] DPI 缩放查询
- [ ] 实现 Windows 平台（复用 SDL3 的话非常简单）
- [ ] 实现 macOS / iOS / Android / Web 平台

---

*Rover Engine Platform v0.1.0 — 文档版本与实现同步。*
