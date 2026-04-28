# 代码风格

> 本规范是 Rover 引擎第一方 C++ 代码必须遵守的风格。命名约定独立见 [`NAMING.md`](NAMING.md)。

---

## 1. 总则

- **C++ 标准**：C++20。允许使用 `concepts`、`<ranges>`、`<bit>`、`std::jthread`、designated initializers
- **格式化**：由 [`.clang-format`](../../.clang-format) 强制执行；提交前必须通过 `./misc/scripts/rover-cli format --check`
- **静态检查**：由 [`.clang-tidy`](../../.clang-tidy) 强制执行；CI 计划开启 `--werror` 模式
- **每行最大长度**：120
- **缩进**：4 spaces，不用 tab
- **行尾**：LF，UTF-8，文件末尾必须有换行

---

## 2. 格式化要点（提取自 .clang-format）

### 2.1 大括号与控制流

- 大括号：基于 `BraceWrapping: Custom`，**类、结构体、函数、命名空间均换行写**
- 控制语句的 `{` **始终另起一行**（`AfterControlStatement: Always`）
- 单语句块也强制大括号（`InsertBraces: true`）

```cpp
// ✅ 推荐
void Window::init()
{
    if (initialized_)
    {
        return;
    }
    for (auto& image : images_)
    {
        process(image);
    }
}

// ❌ 不要
void Window::init() {
    if (initialized_) return;          // 缺大括号 + 单行
    for (auto& image : images_) process(image);
}
```

### 2.2 指针与引用

- `PointerAlignment: Left`：`int* p`，**不**写 `int *p`
- `ReferenceAlignment: Pointer`（跟随指针）：`int& r`
- `QualifierAlignment: Left`：`const int x`，**不**写 `int const x`

```cpp
// ✅
const u32* find_buffer(const std::string& name);
const std::vector<VkImage>& images() const;

// ❌
const u32 *find_buffer(const std::string &name);
std::vector<VkImage> const& images() const;
```

### 2.3 命名空间缩进与注释

- `NamespaceIndentation: All`：命名空间内容缩进
- `FixNamespaceComments: true`：长命名空间末尾自动加 `// namespace rover` 注释

```cpp
// ✅
namespace rover
{

class Foo
{
    // ...
};

} // namespace rover
```

### 2.4 函数参数 / 模板参数

- `BinPackArguments: false`、`BinPackParameters: false`
- 长签名一律 **每个参数一行**

```cpp
// ✅
bool create_pipeline(const PipelineDesc& desc,
                     PipelineHandle*     out,
                     const char*         debug_name);

// ❌
bool create_pipeline(const PipelineDesc& desc, PipelineHandle* out, const char* debug_name);  // 超过 120 列时
```

### 2.5 include 顺序

由 `IncludeBlocks: Regroup` + 三个 priority 自动重排：

| 优先级 | 匹配 | 示例 |
|-------|------|------|
| 1 | `"…"` 引号 | `#include "core/math/vector3.h"` |
| 2 | `<…>` 带 `.h` | `#include <vulkan/vulkan.h>` |
| 3 | `<…>` 不带 `.h`（标准库） | `#include <vector>` |

每组之间空一行，每组内字典序。

```cpp
// ✅ 推荐
#include "core/log/log.h"
#include "core/typedefs.h"
#include "drivers/vulkan/vk_common.h"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <vector>
```

---

## 3. include 路径规则

**强制**：所有 include 使用 **仓库根相对路径**，`rover_add_library()` 已自动把 `ROVER_ROOT_DIR` 加入 `target_include_directories(... PUBLIC)`。

```cpp
// ✅ 正确
#include "core/math/vector3.h"
#include "services/graphics/graphics_service.h"
#include "modules/scene/scene_tree.h"

// ❌ 禁止
#include "../../math/vector3.h"           // 相对路径
#include "vector3.h"                      // 不带目录
```

---

## 4. 头文件规则

### 4.1 守卫

一律用 `#pragma once`，不用三联 `#ifndef`：

```cpp
// ✅
#pragma once

namespace rover
{
// ...
}

// ❌
#ifndef ROVER_CORE_VECTOR3_H_
#define ROVER_CORE_VECTOR3_H_
// ...
#endif
```

### 4.2 头文件中禁止的内容

- `using namespace xxx;`（任何形式）
- 实现性代码（除非是模板或 `inline`）
- 全局变量定义（声明可以，定义放 `.cpp`）
- 不必要的 include（**最小依赖原则**）

### 4.3 前向声明 vs include

- **优先前向声明**：当头文件只需要指针 / 引用 / 函数声明时
- **必须 include**：当头文件需要类型大小（成员、值传递、`sizeof`）

```cpp
// ✅ 前向声明足够
class GraphicsDevice;
class WindowSystem;

class GraphicsService
{
public:
    void init(GraphicsDevice* device, WindowSystem* window);
    GraphicsDevice*  device() const;
    WindowSystem*    window() const;

private:
    GraphicsDevice*  device_  = nullptr;
    WindowSystem*    window_  = nullptr;
};
```

---

## 5. RAII 与资源管理

### 5.1 强制使用 RAII

任何持有资源（GPU 句柄、文件描述符、互斥锁、内存）的对象必须在析构中释放：

```cpp
// ✅ Window 析构自动释放 SDL_Window
Window::~Window()
{
    shutdown();
}

void Window::shutdown()
{
    if (window_ != nullptr)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
}

// ❌ 没有 shutdown，资源泄漏
class Window {
    SDL_Window* window_;
    // 没有析构函数！
};
```

### 5.2 拷贝与移动

- 持有独占资源的类必须删除拷贝（`= delete`）
- 视情况启用移动（如果有意义）；移动后原对象必须处于可析构状态

```cpp
class Window
{
public:
    Window();
    ~Window();

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;
};
```

### 5.3 智能指针

| 用途 | 推荐 |
|------|------|
| 独占所有权 | `std::unique_ptr<T>` |
| 共享所有权（仅在确实必要时） | `std::shared_ptr<T>` |
| 引用计数（引擎对象） | `Ref<T>`（基于 `core/object/RefCounted`） |
| 弱引用 | `std::weak_ptr<T>` 或 `WeakRef<T>` |
| 非拥有原始指针 | 裸 `T*`（明确不持有） |

不要用 `T*` 表示所有权。如果是所有权，必须是 `unique_ptr` 或 `Ref`。

---

## 6. 错误处理

### 6.1 风格

Rover **不使用异常**。所有可失败操作通过：

- 返回 `bool`（成功 / 失败）+ 输出参数
- 返回 `Optional<T>` / `Result<T, E>`（计划，待 core/variant 实现）
- 直接返回值，配合特殊值（如 `INVALID_HANDLE`）

```cpp
// ✅ 当前风格
bool init(const WindowDesc& desc);                      // 失败返回 false 并 log
TextureHandle create_texture(const TextureDesc& desc);  // 失败返回 INVALID_HANDLE

// ❌ 不要抛异常
TextureHandle create_texture(const TextureDesc& desc) {
    if (!valid) throw std::runtime_error("invalid");
}
```

### 6.2 失败时的日志

- 任何 `false` 返回必须先 `ROVER_LOG_ERROR(...)` 说明原因
- Vulkan 调用必须用 `VK_CHECK*` 宏包裹
- 不要静默失败

```cpp
// ✅
bool VkInstanceWrapper::init(const std::vector<const char*>& exts, bool enable_validation)
{
    if (volkInitialize() != VK_SUCCESS)
    {
        ROVER_LOG_ERROR("Failed to initialize volk");
        return false;
    }
    VK_CHECK_RETURN(vkCreateInstance(&create_info, nullptr, &instance_), false);
    return true;
}

// ❌
bool VkInstanceWrapper::init(...) {
    volkInitialize();
    vkCreateInstance(&create_info, nullptr, &instance_);   // 没有错误检查
    return true;
}
```

### 6.3 断言

- 可恢复错误：log + 返回 false
- 编程错误（不应到达）：`assert()` 或 `ROVER_ASSERT`（计划）
- Release 构建中 `assert` 应被禁用，业务代码不应依赖其副作用

---

## 7. const 正确性

- Getter 一律 `const`
- 所有不修改 `*this` 的方法标记 `const`
- 不修改的引用 / 指针参数标记 `const`
- `const` 应当 _左_ 写：`const u32* p`

clang-tidy 中已启用 `readability-make-member-function-const`。

---

## 8. `[[nodiscard]]`

数学/工具类返回值类型一律 `[[nodiscard]]`，防止意外丢弃：

```cpp
struct Vector3
{
    [[nodiscard]] f32   length() const;
    [[nodiscard]] Vector3 normalized() const;
    [[nodiscard]] f32   dot(const Vector3& other) const;
};

class GraphicsDevice
{
public:
    [[nodiscard]] virtual TextureHandle create_texture(const TextureDesc& desc) = 0;
};
```

clang-tidy 中已启用 `modernize-use-nodiscard`。

---

## 9. auto 使用

clang-tidy `modernize-use-auto.MinTypeNameLength = 5`：类型名 ≥ 5 字符时建议用 auto。

- 用 `auto` 时类型推断必须**明显**：迭代器、`make_unique` 返回值、lambda、模板返回值
- 不要为了懒省事用 auto；可读性优先

```cpp
// ✅
for (auto& image : swapchain_.images()) { ... }
auto pipeline = std::make_unique<Pipeline>(desc);
auto* device  = get_vulkan_device();   // RemoveStars=false：保留 *

// ❌（类型不明显）
auto x = compute_thing();
```

---

## 10. 注释

### 规则

- **解释意图，不解释代码做了什么**
- 不写废话注释（`// increment counter`、`// loop over items`）
- 公共 API 头文件可以加 doxygen 风格简短说明，但当前不强制
- TODO / FIXME / NOTE / HACK 标签请加上责任人 / 日期或上下文

```cpp
// ❌ 废话
i++;  // increment i
window_ = SDL_CreateWindow(...);  // create window

// ✅ 解释非显然的意图
// 必须每个交换链图像一个，不是每帧一个 —— 否则 vkQueuePresentKHR 的等待信号量
// 可能在仍被持有时被重用（参考 https://github.com/KhronosGroup/Vulkan-Docs/issues/152）
std::vector<VkSemaphore> render_finished_semaphores_;

// ✅ TODO with context
// TODO(staging-buffer): 当前 update_buffer 仅支持 host-visible；GpuOnly 缓冲需要
// staging buffer 路径，待 Phase 2 落地（见 F-009）
```

---

## 11. 线程安全

### 默认假设

- 引擎对象**默认非线程安全**，由 JobSystem 调度协调
- 真线程安全的类必须在文档（`docs/dev/<area>.md`）显式写明（如 `EventBus::publish`）
- 跨线程共享的类型应使用 `std::atomic` / `std::mutex`，而非"凑巧没冲突"

### 反例

```cpp
// ❌ 在多线程下读 / 写裸成员
class TextureCache {
    std::unordered_map<std::string, TextureHandle> cache_;
public:
    TextureHandle get(const std::string& name) { return cache_[name]; }   // 数据竞争！
};

// ✅
class TextureCache {
    mutable std::shared_mutex                            mutex_;
    std::unordered_map<std::string, TextureHandle>       cache_;
public:
    TextureHandle get(const std::string& name) const
    {
        std::shared_lock lock(mutex_);
        auto it = cache_.find(name);
        return it != cache_.end() ? it->second : INVALID_HANDLE;
    }
};
```

---

## 12. 模板与 `concepts`

- 公共 API 模板应有 `concepts` 约束（C++20）
- 内部模板可以用 SFINAE 但不推荐
- 模板实现放在头文件或 `.inl` 文件，不放 `.cpp`

```cpp
// ✅ 用 concepts 约束
template <typename T>
concept HandleType = requires(T t) {
    { t.value } -> std::convertible_to<u64>;
};

template <HandleType H>
H invalid_handle() { return H{0}; }
```

---

## 13. CI 强制项

PR 合入前必须通过：

- `./misc/scripts/rover-cli format --check`（clang-format 无 drift）
- `./misc/scripts/rover-cli build`（多平台构建通过）
- `./misc/scripts/rover-cli test`（doctest 全绿）
- 计划：`rover lint`（clang-tidy 无错误）

---

## 14. 参考

- [`.clang-format`](../../.clang-format)
- [`.clang-tidy`](../../.clang-tidy)
- [`.editorconfig`](../../.editorconfig)
- [`docs/standard/NAMING.md`](NAMING.md)
- [`docs/standard/ARCHITECTURE_RULES.md`](ARCHITECTURE_RULES.md)
- [`docs/standard/DEBUGGING.md`](DEBUGGING.md)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
