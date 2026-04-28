# 调试与异常处理规范

> 本规范统一 Rover 引擎在 **错误处理**、**日志**、**断言**、**调试开关**、**Sanitizer**、**GPU 验证层** 等方面的约束。

---

## 1. 错误处理风格（强制）

Rover 引擎 **不使用 C++ 异常**。所有可失败操作必须使用以下方式之一：

| 方式 | 用途 | 例子 |
|------|------|------|
| 返回 `bool` + 输出参数 | 二态成功/失败 | `bool init(const WindowDesc& desc)` |
| 返回特殊句柄（如 `INVALID_HANDLE = 0`） | 资源创建 | `BufferHandle create_buffer(...)` |
| 返回 `std::optional<T>` | 值可能不存在 | `std::optional<TextureHandle> find_texture(name)` |
| `Result<T, E>`（待 `core/variant/` 实现） | 富错误信息 | `Result<Pipeline, GfxError> create(...)` |

**不允许**：

```cpp
// ❌ 抛异常
throw std::runtime_error("invalid argument");

// ❌ 静默失败（没有日志）
bool init() { return some_call() == 0; }   // 失败后无任何痕迹

// ❌ exit() / abort() 直接退出（除非编程错误的最后兜底）
if (!init()) std::exit(1);
```

### 失败时的最低义务

任何返回 `false` / `INVALID_HANDLE` 的失败路径**必须**：

1. 调用 `ROVER_LOG_ERROR(...)` 说明失败原因（含上下文）
2. 不留下半初始化的状态（已分配的资源必须清理）
3. 让调用方有机会决定如何处理（不直接 `exit()`）

```cpp
// ✅ 正确
bool VkInstanceWrapper::init(const std::vector<const char*>& exts, bool validation)
{
    if (volkInitialize() != VK_SUCCESS)
    {
        ROVER_LOG_ERROR("Failed to initialize volk loader");
        return false;
    }
    
    VkResult result = vkCreateInstance(&info, nullptr, &instance_);
    if (result != VK_SUCCESS)
    {
        ROVER_LOG_ERROR("vkCreateInstance failed: {}", static_cast<int>(result));
        return false;
    }
    
    return true;
}
```

---

## 2. 日志规范（强制）

### 2.1 双通道

| 通道 | 宏前缀 | 用途 |
|------|--------|------|
| `Rover` | `ROVER_LOG_*` | 引擎内部日志（core / drivers / platform / services / modules） |
| `App` | `ROVER_APP_*` | 游戏 / 应用层日志（用户代码用） |

第一方代码 **永远** 用 `ROVER_LOG_*`。`ROVER_APP_*` 仅供下游用户。

### 2.2 级别选择

| 级别 | 何时使用 | Release 行为 |
|------|---------|-------------|
| `TRACE` | 详细执行轨迹（每帧调用、每个分支） | **编译为 `(void)0`** |
| `DEBUG` | 开发期诊断信息 | **编译为 `(void)0`** |
| `INFO` | 关键生命周期事件（init / shutdown / 大状态切换） | 输出 |
| `WARN` | 异常但非致命（资源缺失用 fallback、降级） | 输出 |
| `ERROR` | 操作失败但引擎可继续运行 | 输出 |
| `FATAL` | 引擎无法继续，调用方应该终止 | 输出 |

### 2.3 反例

```cpp
// ❌ 不要：在 hot path 用 INFO
for (auto& obj : scene)
{
    ROVER_LOG_INFO("Processing {}", obj.name);   // 每帧上千条 INFO
    process(obj);
}

// ✅ 改用 TRACE（Release 自动禁用）
for (auto& obj : scene)
{
    ROVER_LOG_TRACE("Processing {}", obj.name);
    process(obj);
}

// ❌ 不要：拼接字符串再传入
std::string msg = "Failed to load: " + name + " err=" + std::to_string(code);
ROVER_LOG_ERROR(msg);

// ✅ 用 fmt 格式化
ROVER_LOG_ERROR("Failed to load: {} err={}", name, code);
```

### 2.4 日志格式

`Log::init()` 配置的格式：`[HH:MM:SS.mmm] [LoggerName] [level] message`。不要在 message 里再加时间或级别。

---

## 3. 断言

当前引擎**没有**统一断言宏（计划添加 `ROVER_ASSERT`）。临时使用：

```cpp
#include <cassert>

// 编程错误（不应到达）
assert(handle != INVALID_HANDLE && "buffer must be created before use");

// 不要用 assert 检查可恢复错误（如用户输入）
```

**未来**（计划）：
- `ROVER_ASSERT(cond, msg)` —— Debug 仅
- `ROVER_VERIFY(cond, msg)` —— Debug + Release（执行表达式但 Release 不检查）
- `ROVER_FATAL_IF(cond, msg)` —— 始终检查，触发即 fatal log + abort

---

## 4. Vulkan 验证层

### 4.1 启用条件

`drivers/vulkan/graphics_device_vulkan_lifecycle.cpp::init` 中：

```cpp
#ifdef ROVER_DEBUG
constexpr bool enable_validation = true;
#else
constexpr bool enable_validation = false;
#endif
```

`Rover::CompileFlags` 在 Debug 构建注入 `ROVER_DEBUG`。

### 4.2 优雅降级

如果系统未安装 `VK_LAYER_KHRONOS_validation`：

- 记录 WARN 但 **不致命**（验证层缺失不应阻止开发者跑引擎）
- driver 自动以 `enable_validation = false` 模式继续

### 4.3 启用方式

| 场景 | 步骤 |
|------|------|
| 安装到系统路径 | `apt install vulkan-validationlayers`（Debian/Ubuntu）/ Vulkan SDK 安装包 |
| 通过环境变量 | `VK_LAYER_PATH=/path/to/sdk/share/vulkan/explicit_layer.d VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./bin/debug/rover` |
| 通过 rover-cli | `./misc/scripts/rover-cli run --validation`（自动注入环境变量） |

### 4.4 调试消息映射

`VkDebugUtilsMessengerEXT` 回调把 Vulkan 消息转发到引擎日志：

| Vulkan severity | 引擎日志 |
|-----------------|---------|
| `VERBOSE` / `INFO` | `ROVER_LOG_TRACE` |
| `WARNING` | `ROVER_LOG_WARN` |
| `ERROR` | `ROVER_LOG_ERROR` |

不要在 hot path 中触发验证层 ERROR 路径（性能极差）；遇到 ERROR 应当 RenderDoc 抓帧诊断。

---

## 5. Sanitizer

### 5.1 选项

由 `Rover::CompileFlags` 在 Debug 构建中按需注入：

| 选项 | 等效 flag | 用途 |
|------|----------|------|
| `ROVER_ENABLE_ASAN`  | `-fsanitize=address` | 内存越界 / use-after-free |
| `ROVER_ENABLE_UBSAN` | `-fsanitize=undefined` | 未定义行为（整数溢出、空指针解引用等） |
| `ROVER_ENABLE_TSAN`  | `-fsanitize=thread` | 数据竞争 |

### 5.2 互斥关系

- ASan ⊥ TSan：不可同时启用（运行时会冲突）
- UBSan 可与 ASan **或** TSan 任意一个组合
- Release 构建禁用所有 sanitizer

### 5.3 启用方式

```bash
# 通过 rover-cli
./misc/scripts/rover-cli configure --asan
./misc/scripts/rover-cli configure --ubsan
./misc/scripts/rover-cli configure --tsan      # 与 asan 互斥

./misc/scripts/rover-cli build
./misc/scripts/rover-cli run
```

### 5.4 何时跑 Sanitizer

| 触发 | 应跑 |
|------|------|
| 修改了 `core/allocator/` | ASan（内存安全） |
| 修改了 `core/task/` | TSan（线程安全） |
| Crash 复现 | ASan + UBSan 一起 |
| 周期 CI | 每天一次 ASan + UBSan 全套测试 |

---

## 6. 调试器（gdb）

### 6.1 通过 rover-cli

```bash
# 交互模式
./misc/scripts/rover-cli debug

# 一次性捕获崩溃栈（CI 友好）
./misc/scripts/rover-cli debug --batch

# 在符号处下断点
./misc/scripts/rover-cli debug --break 'rover::run_main_loop'

# 启用 Vulkan 验证层 + gdb 一起
./misc/scripts/rover-cli debug --validation
```

### 6.2 推荐 gdb 配置

`~/.gdbinit` 或项目级 `.gdbinit`：

```
set print pretty on
set print object on
set pagination off
```

### 6.3 GDB pretty-printer

考虑加入：
- glm 类型（vector / matrix）
- `Vector3` / `Mat4` / `Quat`
- `Ref<T>`、`Object*`

待 Phase 2 实现 Variant 时一并加入 `misc/scripts/gdb/`（计划）。

---

## 7. RenderDoc / GPU 调试

### 7.1 抓帧

在 Linux 上：

```bash
# 安装 RenderDoc 后
./renderdoccmd capture-options --capture-callstacks=on
./renderdoccmd capture --working-dir $PWD ./bin/debug/rover
```

或图形界面：File → Launch Application → 选择 `bin/debug/rover`。

### 7.2 调试标签（计划）

未来在 driver 层加入 `vkCmdBeginDebugUtilsLabelEXT` / `vkCmdEndDebugUtilsLabelEXT`，让 RenderDoc 显示渲染阶段名（如 "Geometry Pass"、"Lighting Pass"），见 [REQUIREMENTS.md NF-205](../product/REQUIREMENTS.md)。

---

## 8. 性能调试

### 8.1 计时

- 帧时间通过 `TimeSource::delta_seconds()` 获取
- 局部段计时建议用 `core/time/` 的工具（如未来的 `ScopedTimer`）

### 8.2 GPU 计时（计划）

通过 Vulkan timestamp query。待 Phase 2 实现，见 [REQUIREMENTS.md NF-206](../product/REQUIREMENTS.md)。

### 8.3 perf / VTune

Linux 下：

```bash
perf record -g ./bin/release/rover
perf report
```

构建时启用 `-fno-omit-frame-pointer`（Release 默认未启用，按需开启）。

---

## 9. CI 中的调试构建

GitHub Actions（计划）：

| 矩阵维度 | 值 |
|---------|---|
| 平台 | Linux / Windows / macOS |
| 构建类型 | Debug / Release |
| Sanitizer | none / ASan+UBSan |
| 后端 | Vulkan（始终） |

每次 PR 至少跑 Linux Debug + ASan，主分支跑全矩阵。

---

## 10. 异常处理流程图

```
                    操作执行
                       │
                ┌──────┴──────┐
                ▼             ▼
            成功？           失败？
                │              │
                │              ▼
                │     ROVER_LOG_ERROR(原因 + 上下文)
                │              │
                │              ▼
                │     清理已分配资源（RAII / shutdown）
                │              │
                │              ▼
                │     返回 false / INVALID_HANDLE
                │              │
                └──────┬───────┘
                       ▼
                调用方决定：
                  - 局部恢复（例如使用 fallback 资源）
                  - 上传错误并 return false
                  - 极端：log FATAL 然后退出（仅 main）
```

---

## 11. 调试开关速查

| 开关 | 位置 | 默认 | 影响 |
|------|------|------|------|
| `CMAKE_BUILD_TYPE` | CMake | `Debug` | 全引擎编译选项 |
| `ROVER_DEBUG` 宏 | `Rover::CompileFlags` | Debug 构建定义 | 验证层启用、TRACE/DEBUG 日志 |
| `ROVER_RELEASE` 宏 | `Rover::CompileFlags` | Release 构建定义 | 优化、移除调试信息 |
| `ROVER_WARNINGS_AS_ERRORS` | CMake option | `OFF` | `-Werror` / `/WX` |
| `ROVER_ENABLE_ASAN/UBSAN/TSAN` | CMake option | `OFF` | Sanitizer 注入 |
| `VK_INSTANCE_LAYERS` | env | unset | 启用 Vulkan 验证层 |
| `VK_LAYER_PATH` | env | unset | 验证层 dlopen 路径 |
| `NO_COLOR` | env | unset | 禁用日志彩色输出 |

---

## 12. 参考

- [`docs/standard/CODE_STYLE.md`](CODE_STYLE.md) §6 错误处理
- [`docs/dev/CORE.md`](../dev/CORE.md) §4 日志系统
- [`docs/dev/DRIVERS.md`](../dev/DRIVERS.md) §14 验证层启用
- [`docs/dev/MISC.md`](../dev/MISC.md) §5.4 / §5.5 run / debug 子命令
- [Vulkan Validation Layers](https://github.com/KhronosGroup/Vulkan-ValidationLayers)
- [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)
- [ThreadSanitizer](https://clang.llvm.org/docs/ThreadSanitizer.html)
