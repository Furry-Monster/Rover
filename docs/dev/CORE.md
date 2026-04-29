# Rover 引擎 Core 子系统文档

> **权威文档** — 对 `core/` 的任何修改都 **必须** 同步更新本文档。修改前请先阅读本文档了解现有设计。

---

## 目录

1. [概述](#1-概述)
2. [文件清单](#2-文件清单)
3. [基础类型 — `core/typedefs.h`](#3-基础类型--coretypedefsh)
4. [日志系统 — `core/log/`](#4-日志系统--corelog)
5. [数学库 — `core/math/`](#5-数学库--coremath)
6. [内存分配器 — `core/allocator/`](#6-内存分配器--coreallocator)
7. [事件系统 — `core/event/`](#7-事件系统--coreevent)
8. [任务系统 — `core/task/`](#8-任务系统--coretask)
9. [对象模型 — `core/object/`](#9-对象模型--coreobject)
10. [图形设备抽象 — `core/graphics/`](#10-图形设备抽象--coregraphics)
11. [注册与生命周期](#11-注册与生命周期)
12. [依赖关系](#12-依赖关系)
13. [线程安全性](#13-线程安全性)
14. [编码约定](#14-编码约定)
15. [后续工作](#15-后续工作)

---

## 1. 概述

`core/` 是 Rover 引擎的 **Layer 1**，为整个引擎提供基础设施。它 **只依赖** `vendor/` 中的第三方库（spdlog、EnTT），**绝不** 依赖 `drivers/`、`services/`、`modules/` 或 `editor/`。

Core 包含 7 个子系统：

| 子系统 | 目录 | 职责 |
|--------|------|------|
| **日志** | `core/log/` | spdlog 封装，引擎/游戏双通道日志 |
| **数学** | `core/math/` | 向量、矩阵、四元数、AABB、变换（手写，列主序，Vulkan NDC Z∈[0,1]） |
| **分配器** | `core/allocator/` | 线性分配器、池分配器、Arena 分配器 |
| **事件** | `core/event/` | Delegate、Signal、EventBus |
| **任务** | `core/task/` | 基于 std::jthread 的 Job System（work-stealing） |
| **对象模型** | `core/object/` | Object 基类、ClassDB 反射注册、RefCounted |
| **图形抽象** | `core/graphics/` | GraphicsDevice 纯虚接口、GPU 资源句柄与描述符 |

CMake target：`rover_core`（别名 `Rover::Core`），类型 STATIC，依赖 `spdlog`、`entt`。

---

## 2. 文件清单

```
core/
├── typedefs.h                          基础类型别名
├── register_core_types.{h,cpp}         注册/注销入口
├── CMakeLists.txt                      构建配置
│
├── log/
│   ├── log.h                           Log 类 + 日志宏
│   └── log.cpp                         Log 实现
│
├── math/
│   ├── math_defs.h                     常量与工具函数
│   ├── vector2.h                       Vector2
│   ├── vector3.h                       Vector3
│   ├── vector4.h                       Vector4
│   ├── mat4.h / mat4.cpp               Mat4（透视投影 RH，clip Z [0,1]）
│   ├── quat.h                          Quat（`.v` 为 x,y,z,w）
│   ├── aabb.h                          AABB（轴对齐包围盒）
│   ├── transform3d.h                   Transform3D（位置/旋转/缩放）
│   └── math.h                          伞形头文件
│
├── allocator/
│   ├── linear_allocator.{h,cpp}        线性/帧分配器
│   ├── pool_allocator.{h,cpp}          固定大小块池
│   ├── arena_allocator.{h,cpp}         可增长 Arena
│   └── allocator.h                     伞形头文件
│
├── event/
│   ├── delegate.h                      类型擦除可调用对象（SBO）
│   ├── signal.h                        信号-槽观察者模式
│   ├── event_bus.{h,cpp}               类型键控事件总线
│   └── event.h                         伞形头文件
│
├── task/
│   ├── thread_safe_queue.h             线程安全 MPMC 队列
│   ├── work_stealing_queue.h           Chase-Lev 工作窃取双端队列
│   ├── job_handle.h                    Job 完成句柄
│   ├── job_system.{h,cpp}             JobSystem 主类
│   └── task.h                          伞形头文件
│
├── object/
│   ├── property_info.h                 StringName、PropertyType、PropertyInfo
│   ├── object_macros.h                 ROVER_CLASS 宏
│   ├── object.{h,cpp}                 Object 基类
│   ├── class_db.{h,cpp}              ClassDB 类型注册表
│   └── ref_counted.{h,cpp}           RefCounted 引用计数基类
│
└── graphics/
    ├── graphics_types.h                GPU 句柄类型、枚举、标志位
    ├── graphics_desc.h                 资源描述符结构体
    ├── graphics_device.h               GraphicsDevice 纯虚接口
    ├── window_system.h                 WindowSystem 抽象（连接 platform 与 driver）
    └── graphics.h                      伞形头文件
```

共 **45 个文件**（35 个 `.h` + 10 个 `.cpp`）。

---

## 3. 基础类型 — `core/typedefs.h`

为整个引擎提供统一的固定宽度类型别名，避免直接使用 `uint32_t` 等冗长名称。

### 类型映射

| 别名 | 原型 | 说明 |
|------|------|------|
| `u8`, `u16`, `u32`, `u64` | `std::uint8_t` … `std::uint64_t` | 无符号整数 |
| `i8`, `i16`, `i32`, `i64` | `std::int8_t` … `std::int64_t` | 有符号整数 |
| `f32` | `float` | 单精度浮点 |
| `f64` | `double` | 双精度浮点 |
| `usize` | `std::size_t` | 无符号大小 |
| `isize` | `std::ptrdiff_t` | 有符号大小 |

所有类型位于 `namespace rover` 中。全引擎通过 `#include "core/typedefs.h"` 统一引入。

---

## 4. 日志系统 — `core/log/`

### 4.1 架构

基于 **spdlog** 封装，提供两个独立日志通道：

| 日志器名称 | 用途 | 宏前缀 |
|-----------|------|--------|
| `"Rover"` | 引擎内部日志 | `ROVER_LOG_*` |
| `"App"` | 游戏/应用层日志 | `ROVER_APP_*` |

两个日志器都使用 **stdout 彩色 sink**，输出格式为：
```
[HH:MM:SS.mmm] [LoggerName] [level] message
```

### 4.2 类 — `Log`

```cpp
class Log {
public:
    Log() = delete;                                   // 纯静态，不可实例化
    static void init();                               // 创建两个 spdlog logger
    static void shutdown();                           // 重置 logger 并调用 spdlog::shutdown()
    static std::shared_ptr<spdlog::logger>& get_core_logger();
    static std::shared_ptr<spdlog::logger>& get_app_logger();
};
```

### 4.3 宏

| 宏 | 级别 | Release 行为 |
|----|------|-------------|
| `ROVER_LOG_TRACE(...)` | trace | 编译为 `(void)0` |
| `ROVER_LOG_DEBUG(...)` | debug | 编译为 `(void)0` |
| `ROVER_LOG_INFO(...)` | info | 正常输出 |
| `ROVER_LOG_WARN(...)` | warn | 正常输出 |
| `ROVER_LOG_ERROR(...)` | error | 正常输出 |
| `ROVER_LOG_FATAL(...)` | critical | 正常输出 |

`ROVER_APP_*` 系列宏具有相同的级别和 Release 行为，使用 App logger。

### 4.4 生命周期

- **初始化**：`register_core_types()` 中第一个调用 `Log::init()`
- **关闭**：`unregister_core_types()` 中最后一个调用 `Log::shutdown()`
- 在 `Log::init()` 之前或 `Log::shutdown()` 之后调用日志宏是 **未定义行为**

### 4.5 使用示例

```cpp
#include "core/log/log.h"

ROVER_LOG_INFO("Engine started, version {}.{}", major, minor);
ROVER_LOG_WARN("Texture {} not found, using fallback", name);
ROVER_APP_DEBUG("Player position: ({}, {}, {})", x, y, z);
```

---

## 5. 数学库 — `core/math/`

### 5.1 设计原则

- **手写标量实现**：向量 / 四元数为头文件；`Mat4` 的求逆与部分工厂放在 `mat4.cpp`，避免符号膨胀。
- **列主序**：与 Vulkan、着色器约定一致；`Mat4::perspective` / `ortho` 输出 **clip-space Z ∈ [0, 1]**（Vulkan）。
- **四元数布局**：与 Godot 一致，分量存放在 `.v.x`/`.v.y`/`.v.z`/`.v.w`。
- **`[[nodiscard]]`**：返回值的方法标记 `[[nodiscard]]` 防止忽略结果
- **Trivially copyable**：向量 / Quat / Mat4 可安全 memcpy / POD 用法

### 5.2 常量与工具 — `math_defs.h`

| 常量 | 类型 | 值 |
|------|------|-----|
| `PI` | `f64` | 3.14159265358979323846 |
| `TAU` | `f64` | 2π |
| `EPSILON` | `f64` | 1e-6 |
| `DEG_TO_RAD` | `f64` | π/180 |
| `RAD_TO_DEG` | `f64` | 180/π |

| 函数 | 签名 | 说明 |
|------|------|------|
| `lerp` | `T lerp(T a, T b, T t)` | 线性插值 |
| `clamp` | `T clamp(T value, T lo, T hi)` | 区间钳位 |
| `is_nearly_equal` | `bool (f64, f64, f64 tol)` | 浮点近似相等 |
| `is_nearly_zero` | `bool (f64, f64 tol)` | 浮点近似为零 |

### 5.3 Vector2 — `vector2.h`

```cpp
struct Vector2 {
    struct { float x, y; } v{};
    // 构造：默认(0,0)、(x,y)
    // 访问器：x(), y()
    // …
};
```

### 5.4 Vector3 — `vector3.h`

与 Vector2 同构，增加 z 分量。

- `cross()` 返回 `Vector3`（右手系）
- 静态常量 `FORWARD{0,0,-1}`、`BACK{0,0,1}`

### 5.5 Vector4 — `vector4.h`

与 Vector3 同构，增加 w；提供 `operator[](0..3)` 便于矩阵乘法。

### 5.6 Mat4 — `mat4.h` / `mat4.cpp`

```cpp
struct Mat4 {
    float c[4][4]; // columns[col][row]

    constexpr Vector4 operator*(const Vector4&) const;
    Mat4 inverse() const; // Gauss–Jordan，实现在 mat4.cpp
    static Mat4 perspective(...); // RH，Vulkan Z
    ...
};
```

### 5.7 Quat — `quat.h`

```cpp
struct Quat {
    struct { float x, y, z, w; } v{}; // Godot 顺序：向量 part + 标量 w

    Quat(float scalar_w, float vx, float vy, float vz); // 兼容旧调用顺序 (w,x,y,z)
    Vector3 operator*(const Vector3&) const; // v + 2*(q_vec × (q_vec × v)) …
    ...
};
```

### 5.8 AABB — `aabb.h`

```cpp
struct AABB {
    Vector3 min{};  // 最小角
    Vector3 max{};  // 最大角

    Vector3 get_center() const;     // (min + max) / 2
    Vector3 get_size() const;       // max - min
    bool contains(Vector3) const;   // 点包含测试
    bool intersects(AABB) const;    // 盒子相交测试
    AABB merge(AABB) const;         // 合并两个 AABB
    void expand(Vector3);           // 扩展以包含点
};
```

### 5.9 Transform3D — `transform3d.h`

```cpp
struct Transform3D {
    Vector3 origin{};                    // 位置
    Quat    rotation{};                  // 旋转
    Vector3 scale{1.0f, 1.0f, 1.0f};    // 缩放

    Mat4 to_mat4() const;               // T * R * S 组合矩阵
    Transform3D inverse() const;
    Vector3 transform_point(Vector3) const;      // origin + rotation * (point * scale)
    Vector3 transform_direction(Vector3) const;  // rotation * dir（忽略平移和缩放）
    static Transform3D identity();
};
```

### 5.10 伞形头文件 — `math.h`

```cpp
#include "core/math/math_defs.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/math/mat4.h"
#include "core/math/quat.h"
#include "core/math/aabb.h"
#include "core/math/transform3d.h"
```

---

## 6. 内存分配器 — `core/allocator/`

### 6.1 设计原则

- **非多态**：具体类型，无虚函数，零开销
- **非线程安全**：调用者需自行同步（文档中有 assert 保护）
- **不可复制，可移动**：拥有唯一内存资源
- **对齐**：所有 `allocate()` 支持自定义对齐（默认 `alignof(std::max_align_t)`）

### 6.2 LinearAllocator — 线性/帧分配器

```cpp
class LinearAllocator {
    explicit LinearAllocator(usize capacity);

    void* allocate(usize size, usize alignment = alignof(std::max_align_t));
    void reset();                        // 重置偏移到 0，不释放内存
    template<typename T, typename... Args>
    T* construct(Args&&...);             // allocate + placement new

    usize used() const;
    usize capacity() const;
};
```

**用途**：帧临时分配。每帧开始时调用 `reset()`，帧内 bump 分配。渲染命令收集的关键组件。

**内部实现**：单一 `std::unique_ptr<u8[]>` 缓冲区 + 偏移指针。对齐通过 `(offset + alignment - 1) & ~(alignment - 1)` 实现。空间不足时返回 `nullptr`。

### 6.3 PoolAllocator — 固定大小块池

```cpp
class PoolAllocator {
    PoolAllocator(usize block_size, usize block_count);

    void* allocate();                    // 从 free list 弹出
    void deallocate(void* ptr);          // 推回 free list

    usize free_count() const;
    usize capacity() const;
};
```

**用途**：大量相同大小对象的高效分配/释放（如粒子、ECS 组件）。

**内部实现**：单一缓冲区分成等大小块。空闲块通过 **嵌入式 free list** 链接（`FreeNode` 指针嵌入在空闲块的前 8 字节）。`block_size` 会自动提升到不低于 `sizeof(FreeNode)`。

### 6.4 ArenaAllocator — 可增长 Arena

```cpp
class ArenaAllocator {
    explicit ArenaAllocator(usize chunk_size = 64 * 1024);

    void* allocate(usize size, usize alignment = alignof(std::max_align_t));
    void reset();                        // 重置所有 chunk 偏移，保留内存
    void release();                      // 释放所有内存
    template<typename T, typename... Args>
    T* construct(Args&&...);
};
```

**用途**：生命周期统一的批量分配（如关卡加载期间的资源）。

**内部实现**：维护 `std::vector<Chunk>`（每个 Chunk 含 `unique_ptr<u8[]>` + size + offset）。当前 chunk 空间不足时，扫描后续 chunk 或分配新 chunk。超大分配（> chunk_size）会创建专用 chunk。

### 6.5 伞形头文件 — `allocator.h`

包含三个分配器头文件。

---

## 7. 事件系统 — `core/event/`

### 7.1 架构概览

三层设计：

```
Delegate  →  Signal  →  EventBus
(可调用对象)   (观察者)   (类型路由)
```

### 7.2 Delegate — `delegate.h`

类型擦除的可调用对象封装，类似轻量 `std::function`，但针对引擎场景优化。

```cpp
template<typename Ret, typename... Args>
class Delegate<Ret(Args...)> {
    // SBO：32 字节内联存储，超过时堆分配
    // 移动语义，不可复制

    Ret operator()(Args...);
    explicit operator bool() const;
    void reset();

    template<auto FnPtr>
    static Delegate from_function();                  // 编译期函数指针，零存储

    template<auto MethodPtr, typename T>
    static Delegate from_method(T* obj);              // 成员函数 + 对象指针
};
```

**SBO 策略**：

| 条件 | 存储方式 |
|------|---------|
| `sizeof(Callable) <= 32` 且 `alignof <= max_align_t` 且 nothrow-move | 内联（SBO） |
| 其他 | 堆分配 |

**典型尺寸分析**：
- 函数指针：8 字节 → SBO
- 无捕获 lambda：1 字节 → SBO
- 捕获 1-3 个指针的 lambda：8-24 字节 → SBO
- `from_function<>()` 和 `from_method<>()`：0 或 8 字节 → SBO

### 7.3 Signal — `signal.h`

```cpp
template<typename... Args>
class Signal {
    SlotId connect(Delegate<void(Args...)> slot);  // 返回 ID
    void disconnect(SlotId id);
    void disconnect_all();
    void emit(Args... args);                       // 调用所有已连接槽
    usize slot_count() const;
};
```

- `SlotId` 为 `u32`，自增分配
- 内部存储：`std::vector<std::pair<SlotId, Delegate<void(Args...)>>>`
- **非线程安全**；emit 期间 connect/disconnect 是 **未定义行为**

### 7.4 EventBus — `event_bus.h`

```cpp
class EventBus {
    template<typename EventType>
    SlotId subscribe(Delegate<void(const EventType&)> slot);

    template<typename EventType>
    void unsubscribe(SlotId id);

    template<typename EventType>
    void publish(const EventType& event);
};
```

- **非单例**：通过引用传递，便于测试
- 使用 `std::type_index` 作为事件类型的键
- 内部为每种事件类型维护一个 `Signal<const EventType&>`
- 类型擦除通过 `ErasedSignal`（内含 `void*` + deleter）实现

### 7.5 使用示例

```cpp
// 定义事件
struct WindowResizeEvent {
    u32 width;
    u32 height;
};

// 订阅
EventBus bus;
auto id = bus.subscribe<WindowResizeEvent>(
    Delegate<void(const WindowResizeEvent&)>([](const WindowResizeEvent& e) {
        ROVER_LOG_INFO("Window resized to {}x{}", e.width, e.height);
    })
);

// 发布
bus.publish(WindowResizeEvent{1920, 1080});

// 取消订阅
bus.unsubscribe<WindowResizeEvent>(id);
```

---

## 8. 任务系统 — `core/task/`

### 8.1 架构概览

```
JobSystem（主调度器）
├── WorkStealingQueue × N  （每个 worker 线程一个本地队列）
├── ThreadSafeQueue         （全局溢出队列）
└── std::jthread × N       （worker 线程，N = 硬件线程 - 1）
```

### 8.2 ThreadSafeQueue — `thread_safe_queue.h`

```cpp
template<typename T>
class ThreadSafeQueue {
    void push(T item);
    bool try_pop(T& out);              // 非阻塞
    bool wait_pop(T& out);             // 阻塞，shutdown 时返回 false
    void shutdown();                   // 唤醒所有等待线程
    bool is_empty() const;
    usize size() const;
};
```

基于 `std::mutex` + `std::condition_variable` 的 MPMC 队列。

### 8.3 WorkStealingQueue — `work_stealing_queue.h`

**Chase-Lev 工作窃取双端队列**：

```cpp
template<typename T>
class WorkStealingQueue {
    explicit WorkStealingQueue(usize capacity);  // 必须是 2 的幂

    void push(T item);          // 所有者线程：推入 bottom（LIFO，缓存热）
    bool try_pop(T& out);       // 所有者线程：从 bottom 弹出
    bool try_steal(T& out);     // 任意线程：从 top 窃取（FIFO）
};
```

- 使用 `std::atomic<i64>` 的 top/bottom 索引
- 固定容量 ring buffer（2 的幂 + 位掩码）
- 内存序：push 使用 release fence，steal 使用 acquire + seq_cst CAS

### 8.4 JobHandle — `job_handle.h`

```cpp
class JobHandle {
    bool is_complete() const;           // 原子轮询
    void wait() const;                  // spin-wait with yield
    // 内部：shared_ptr<atomic<bool>>
};
```

### 8.5 JobSystem — `job_system.h`

```cpp
using Job = std::function<void()>;

class JobSystem {
    explicit JobSystem(u32 thread_count = 0);  // 0 = hardware_concurrency - 1
    ~JobSystem();                              // 自动 shutdown

    void init();                               // 创建 worker 线程
    void shutdown();                           // 停止并 join 所有线程

    void      submit(Job job);                 // 提交 job
    JobHandle submit_with_handle(Job job);     // 提交并返回可等待句柄

    void wait(const JobHandle& handle);        // 等待特定 job（同时执行其他 job）
    void wait_all();                           // 等待所有 in-flight job

    u32 worker_count() const;
};
```

### 8.6 调度策略

**submit 路由**：
1. 如果调用者是 worker 线程 → 推入本地 `WorkStealingQueue`
2. 否则 → 推入全局 `ThreadSafeQueue`

**worker 循环优先级**：
1. 自己的本地队列（`try_pop`，LIFO，缓存热）
2. 随机选择其他 worker 窃取（`try_steal`，FIFO）
3. 全局队列（`try_pop`，非阻塞）
4. 全局队列（`wait_pop`，阻塞等待，省 CPU）

**wait 语义**：`wait()` 和 `wait_all()` 在等待期间会**主动执行其他 job**（productive waiting），避免浪费 CPU 时间。

### 8.7 线程模型

- 使用 C++20 `std::jthread` + `std::stop_token` 实现干净关闭
- `thread_local u32 t_worker_index` 标识当前线程身份
- 默认 worker 数 = `hardware_concurrency - 1`（留一个给主线程）
- 本地队列容量：1024（`kLocalQueueCapacity`）
- `jobs_in_flight_` 原子计数器追踪未完成 job 数量

---

## 9. 对象模型 — `core/object/`

### 9.1 架构概览

```
PropertyInfo / StringName    ← 基础元数据
        ↓
    Object                   ← 所有引擎对象的基类
        ↓
    RefCounted               ← 引用计数子类（资源等）
        ↓
    ClassDB                  ← 全局类型注册表（工厂模式）
```

### 9.2 StringName — `property_info.h`

```cpp
using StringName = std::string;
```

当前为 `std::string` 的简单别名。**后续优化方向**：改为 interned string（字符串驻留），使比较操作从 O(n) 变为 O(1)。修改此类型定义即可全局生效。

### 9.3 PropertyType — `property_info.h`

```cpp
enum class PropertyType : u8 {
    Bool, Int, Float, String,
    Vector2, Vector3, Vector4, Quat, Mat4,
    Object, Unknown
};

struct PropertyInfo {
    StringName   name;
    PropertyType type = PropertyType::Unknown;
    StringName   hint;
};
```

用于反射系统中描述属性元数据。当前仅声明，属性绑定功能将在后续迭代中实现。

### 9.4 Object — `object.h`

```cpp
class Object {
public:
    Object();                                        // 分配唯一 instance_id
    virtual ~Object() = default;

    virtual StringName get_class_name() const;       // 返回 "Object"
    static StringName get_class_name_static();       // 返回 "Object"
    static StringName get_parent_class_name_static();// 返回 ""

    virtual bool is_class(const StringName& name) const;  // name == "Object"

    u64 get_instance_id() const;                     // 全局唯一 ID
};
```

- 实例 ID 通过 `std::atomic<u64>` 自增分配，全局唯一
- `get_class_name()` 和 `is_class()` 为虚函数，子类通过 `ROVER_CLASS` 宏覆盖

### 9.5 ROVER_CLASS 宏 — `object_macros.h`

```cpp
#define ROVER_CLASS(m_class, m_parent)                                          \
public:                                                                         \
    static StringName get_class_name_static() { return #m_class; }              \
    static StringName get_parent_class_name_static() { return #m_parent; }      \
    StringName get_class_name() const override { return #m_class; }             \
    bool is_class(const StringName& p_name) const override {                    \
        if (p_name == #m_class) return true;                                    \
        return m_parent::is_class(p_name);                                      \
    }                                                                           \
private:
```

**使用方式**：
```cpp
class MyNode : public Object {
    ROVER_CLASS(MyNode, Object)
public:
    // ... 类的其他成员
};
```

宏注入 4 个函数：
1. `get_class_name_static()` — 静态类名
2. `get_parent_class_name_static()` — 静态父类名
3. `get_class_name()` override — 运行时类名
4. `is_class()` override — 递归继承链检查

### 9.6 ClassDB — `class_db.h`

```cpp
struct ClassInfo {
    StringName name;
    StringName parent_name;
    ObjectFactory factory;                            // std::function<Object*()>
    std::unordered_map<StringName, PropertyInfo> properties;
};

class ClassDB {
    static void register_class(const StringName& name, const StringName& parent,
                               ObjectFactory factory);
    template<typename T>
    static void register_class();                     // 自动提取类名和工厂

    static Object* instantiate(const StringName& class_name);
    static bool class_exists(const StringName& name);
    static bool is_parent_class(const StringName& child, const StringName& parent);
    static const ClassInfo* get_class_info(const StringName& name);
};
```

- **全静态类**（`= delete` 构造函数）
- 内部使用 Meyer's singleton：`static std::unordered_map<StringName, ClassInfo>& get_registry()`
- `register_class<T>()` 模板自动调用 `T::get_class_name_static()` 和 `T::get_parent_class_name_static()`
- `is_parent_class()` 通过遍历 `parent_name` 链实现继承关系检查
- 重复注册会触发 assert

### 9.7 RefCounted — `ref_counted.h`

```cpp
class RefCounted : public Object {
    ROVER_CLASS(RefCounted, Object)
public:
    void add_ref();                   // 原子 +1
    bool release();                   // 原子 -1，返回 true 表示计数归零（调用者应 delete）
    u32 ref_count() const;
};
```

- 初始引用计数为 1
- 使用 `std::atomic<u32>` 保证线程安全的引用计数操作
- `release()` 使用 `acq_rel` 内存序确保安全

### 9.8 已注册类型

`register_core_types()` 中注册了两个基础类型：

```cpp
ClassDB::register_class<Object>();
ClassDB::register_class<RefCounted>();
```

---

## 10. 图形设备抽象 — `core/graphics/`

### 10.1 设计原则

这是引擎 **依赖倒置** 的核心体现：

- `core/graphics/` 声明纯虚接口和类型
- `drivers/vulkan/` 实现该接口（`GraphicsDeviceVulkan`）
- `services/graphics/` 仅依赖此抽象，不知道具体后端

### 10.2 句柄类型 — `graphics_types.h`

所有 GPU 资源使用 **不透明 `u64` 句柄**：

| 句柄类型 | 说明 |
|---------|------|
| `BufferHandle` | GPU 缓冲区 |
| `TextureHandle` | 纹理 |
| `SamplerHandle` | 采样器 |
| `ShaderHandle` | 着色器模块 |
| `PipelineHandle` | 图形管线 |
| `RenderPassHandle` | 渲染通道 |
| `FramebufferHandle` | 帧缓冲 |
| `CommandListHandle` | 命令列表 |

`INVALID_HANDLE = 0` 表示无效句柄。

### 10.3 枚举类型 — `graphics_types.h`

**像素/顶点格式 (`Format`)**：
`UNDEFINED`, `R8_UNORM`, `R8G8B8A8_UNORM`, `R8G8B8A8_SRGB`, `B8G8R8A8_UNORM`, `B8G8R8A8_SRGB`, `R16G16B16A16_SFLOAT`, `R32G32B32A32_SFLOAT`, `R32G32B32_SFLOAT`, `R32G32_SFLOAT`, `D32_SFLOAT`, `D24_UNORM_S8_UINT`, `D32_SFLOAT_S8_UINT`

**标志位枚举**（支持 `|`, `&`, `^`, `~`, `|=`, `&=`, `^=`）：

| 枚举 | 标志 |
|------|------|
| `BufferUsage` | Vertex, Index, Uniform, Storage, Indirect, Transfer |
| `TextureUsage` | Sampled, Storage, ColorAttachment, DepthStencilAttachment, TransferSrc, TransferDst |
| `ShaderStage` | Vertex, Fragment, Compute, Geometry, TessControl, TessEval |

**其他枚举**：`PrimitiveTopology`, `CullMode`, `FrontFace`, `CompareOp`, `BlendFactor`, `BlendOp`, `LoadOp`, `StoreOp`, `TextureType`, `Filter`, `SamplerAddressMode`, `IndexType`, `MemoryUsage`

标志位运算符通过 `ROVER_DEFINE_FLAG_OPERATORS` 宏生成。

### 10.4 描述符结构体 — `graphics_desc.h`

资源创建采用 **描述符模式**（类似现代 Vulkan/D3D12 风格）：

| 描述符 | 用途 | 关键字段 |
|--------|------|---------|
| `BufferDesc` | 创建缓冲区 | size, usage, memory, debug_name |
| `TextureDesc` | 创建纹理 | width, height, depth, mip_levels, format, type, usage |
| `SamplerDesc` | 创建采样器 | min/mag_filter, address_u/v/w, max_anisotropy |
| `ShaderDesc` | 创建着色器 | stage, bytecode, bytecode_size, entry_point |
| `ColorAttachmentDesc` | 颜色附件配置 | format, load/store_op, blend 设置 |
| `DepthStencilAttachmentDesc` | 深度附件配置 | format, load/store_op, compare_op, enable 标志 |
| `RenderPassDesc` | 渲染通道 | color_attachments[], depth_stencil |
| `FramebufferDesc` | 帧缓冲 | render_pass, color/depth attachments, width/height |
| `GraphicsPipelineDesc` | 图形管线 | shaders, render_pass, topology, cull, blend, vertex layout |
| `VertexAttribute` | 顶点属性 | location, format, offset |
| `VertexBinding` | 顶点绑定 | binding, stride, attributes[] |
| `Viewport` | 视口 | x, y, width, height, min/max_depth |
| `Scissor` | 裁剪矩形 | x, y, width, height |
| `ClearValue` | 清除值 | color[4], depth, stencil |

默认值已设置合理初始值（如 topology=TriangleList, cull=Back, depth_compare=Less）。

### 10.5 WindowSystem — `window_system.h`

桥接 `platform/` 层与 `drivers/` 层的抽象接口。`platform/<os>/` 实现这个接口（如 Linux 的 `Window` 类），`drivers/<api>/` 通过它创建后端 surface 与查询所需扩展。Vulkan 句柄通过不透明 `void*` 传递，避免 Vulkan 头文件污染 core。

```cpp
class WindowSystem {
public:
    virtual ~WindowSystem() = default;

    // Vulkan 集成
    virtual bool create_vulkan_surface(void* instance, void** surface_out) = 0;
    virtual void get_vulkan_required_extensions(std::vector<const char*>& out) = 0;

    // 窗口属性
    [[nodiscard]] virtual u32  get_width()    const = 0;
    [[nodiscard]] virtual u32  get_height()   const = 0;
    [[nodiscard]] virtual bool should_close() const = 0;
};
```

### 10.6 GraphicsDevice — `graphics_device.h`

纯虚接口，`drivers/` 层实现：

```cpp
class GraphicsDevice {
public:
    virtual ~GraphicsDevice() = default;

    // 生命周期 — init 接收 WindowSystem 引用，driver 通过它创建 surface
    virtual bool init(WindowSystem& window) = 0;
    virtual void shutdown() = 0;

    // 资源创建/销毁
    virtual BufferHandle      create_buffer(const BufferDesc&) = 0;
    virtual void              destroy_buffer(BufferHandle) = 0;
    virtual void*             map_buffer(BufferHandle) = 0;
    virtual void              unmap_buffer(BufferHandle) = 0;
    virtual void              update_buffer(BufferHandle, const void*, usize, usize offset = 0) = 0;
    virtual TextureHandle     create_texture(const TextureDesc&) = 0;
    virtual void              destroy_texture(TextureHandle) = 0;
    virtual void              update_texture(TextureHandle, const void*, usize) = 0;
    virtual SamplerHandle     create_sampler(const SamplerDesc&) = 0;
    virtual void              destroy_sampler(SamplerHandle) = 0;
    virtual ShaderHandle      create_shader(const ShaderDesc&) = 0;
    virtual void              destroy_shader(ShaderHandle) = 0;
    virtual RenderPassHandle  create_render_pass(const RenderPassDesc&) = 0;
    virtual void              destroy_render_pass(RenderPassHandle) = 0;
    virtual FramebufferHandle create_framebuffer(const FramebufferDesc&) = 0;
    virtual void              destroy_framebuffer(FramebufferHandle) = 0;
    virtual PipelineHandle    create_graphics_pipeline(const GraphicsPipelineDesc&) = 0;
    virtual void              destroy_pipeline(PipelineHandle) = 0;

    // 命令录制
    virtual CommandListHandle begin_command_list() = 0;
    virtual void              end_command_list(CommandListHandle) = 0;
    virtual void cmd_begin_render_pass(CommandListHandle, FramebufferHandle,
                                       const ClearValue*, u32 count) = 0;
    virtual void cmd_end_render_pass(CommandListHandle) = 0;
    virtual void cmd_bind_pipeline(CommandListHandle, PipelineHandle) = 0;
    virtual void cmd_set_viewport(CommandListHandle, const Viewport&) = 0;
    virtual void cmd_set_scissor(CommandListHandle, const Scissor&) = 0;
    virtual void cmd_bind_vertex_buffer(CommandListHandle, BufferHandle,
                                        u32 binding = 0, usize offset = 0) = 0;
    virtual void cmd_bind_index_buffer(CommandListHandle, BufferHandle,
                                       IndexType, usize offset = 0) = 0;
    virtual void cmd_draw(CommandListHandle, u32 vertex_count,
                          u32 instance_count = 1, u32 first_vertex = 0,
                          u32 first_instance = 0) = 0;
    virtual void cmd_draw_indexed(CommandListHandle, u32 index_count,
                                  u32 instance_count = 1, u32 first_index = 0,
                                  i32 vertex_offset = 0, u32 first_instance = 0) = 0;

    // 帧管理
    virtual bool begin_frame() = 0;
    virtual void end_frame() = 0;
    virtual void present() = 0;
    virtual void wait_idle() = 0;

    // 交换链查询
    virtual TextureHandle get_swapchain_texture() = 0;        // 当前帧的 image
    virtual u32           get_swapchain_image_count() = 0;    // 交换链图像总数
    virtual TextureHandle get_swapchain_texture_at(u32 i) = 0;// 第 i 个图像（用于预创建 framebuffer）
    virtual Format        get_swapchain_format() = 0;
    virtual u32           get_swapchain_width() = 0;
    virtual u32           get_swapchain_height() = 0;

    // 设备信息
    virtual const char* get_device_name() const = 0;
    virtual const char* get_api_name() const = 0;
};
```

### 10.7 伞形头文件 — `graphics.h`

```cpp
#include "core/graphics/graphics_types.h"
#include "core/graphics/graphics_desc.h"
#include "core/graphics/graphics_device.h"
#include "core/graphics/window_system.h"
```

### 10.8 实现位置

`GraphicsDevice` 与 `WindowSystem` 在 core 中只声明，**不实现**。具体实现：

- **`drivers/vulkan/`** 实现 `GraphicsDevice`（详见 [DRIVERS.md](DRIVERS.md)）
- **`platform/linux/Window`** 实现 `WindowSystem`（详见 [PLATFORM.md](PLATFORM.md)）

依赖倒置在 `main/` 完成具体绑定：driver 通过 `WindowSystem&` 访问平台窗口，无需 include 平台头文件。

---

## 11. 注册与生命周期

### 11.1 初始化顺序

在 `register_core_types()` 中：

```cpp
void register_core_types() {
    Log::init();                          // 1. 日志系统（最先，其他子系统依赖日志）
    ClassDB::register_class<Object>();    // 2. 注册 Object 基类
    ClassDB::register_class<RefCounted>();// 3. 注册 RefCounted
}
```

### 11.2 关闭顺序

在 `unregister_core_types()` 中：

```cpp
void unregister_core_types() {
    Log::shutdown();                      // 最后关闭日志
}
```

### 11.3 不需要显式初始化的子系统

| 子系统 | 原因 |
|--------|------|
| **math** | 纯头文件，无状态 |
| **allocator** | 按需构造实例，无全局状态 |
| **event** | Signal/Delegate 按需构造；EventBus 非全局单例 |
| **task** | JobSystem 由上层（services 或 main）按需创建并 init |
| **graphics** | 纯虚接口，无实现代码 |

---

## 12. 依赖关系

### 12.1 Core 的外部依赖

| 子系统 | 依赖的 vendor 库 |
|--------|-----------------|
| log | spdlog |
| math | （手写；`<cmath>`） |
| allocator | 无（仅标准库） |
| event | 无（仅标准库） |
| task | 无（仅标准库 `<thread>`, `<atomic>`） |
| object | 无（仅标准库） |
| graphics | 无（仅标准库 `<vector>`） |

### 12.2 Core 内部头文件依赖

```
typedefs.h          ← 被所有子系统依赖
    ↓
math_defs.h
    ↓
vector2/3/4.h       ← 依赖 typedefs.h + `<cmath>`
    ↓
mat4.h              ← 依赖 vector3.h, vector4.h
    ↓
quat.h              ← 依赖 vector3.h, mat4.h
    ↓
aabb.h              ← 依赖 vector3.h
    ↓
transform3d.h       ← 依赖 vector3.h, quat.h, mat4.h

property_info.h     ← 定义 StringName, PropertyType
    ↓
object.h            ← 依赖 property_info.h, object_macros.h
    ↓
class_db.h          ← 依赖 property_info.h（前向声明 Object）
    ↓
ref_counted.h       ← 依赖 object.h
```

### 12.3 严禁的依赖方向

`core/` **绝不可以** include：
- `drivers/*`
- `services/*`
- `modules/*`
- `platform/*`
- `editor/*`

---

## 13. 线程安全性

| 类型 | 线程安全 | 说明 |
|------|---------|------|
| `Log` | 是 | spdlog 内部线程安全 |
| `Vector2/3/4`, `Mat4`, `Quat`, `AABB`, `Transform3D` | 是 | 值类型，无共享状态 |
| `LinearAllocator` | **否** | 调用者需同步 |
| `PoolAllocator` | **否** | 调用者需同步 |
| `ArenaAllocator` | **否** | 调用者需同步 |
| `Delegate` | **否** | 移动语义，不共享 |
| `Signal` | **否** | emit 期间禁止 connect/disconnect |
| `EventBus` | **否** | 调用者需同步 |
| `ThreadSafeQueue` | 是 | mutex + condition_variable |
| `WorkStealingQueue` | 部分 | push/pop 仅限所有者线程；steal 任意线程 |
| `JobSystem` | 是 | submit/wait 可从任意线程调用 |
| `JobHandle` | 是 | 原子 bool |
| `Object::instance_id` | 是 | 原子自增 |
| `RefCounted` 引用计数 | 是 | 原子操作 |
| `ClassDB` | **否** | 仅在初始化期间（单线程）调用 register |

---

## 14. 编码约定

详见 [ARCHITECTURE.md §8](ARCHITECTURE.md)，此处补充 Core 相关细节：

| 约定 | 规则 |
|------|------|
| 头文件守卫 | `#pragma once` |
| 命名空间 | `namespace rover { ... }` |
| include 风格 | 仓库根相对：`"core/math/vector3.h"` |
| 类型命名 | PascalCase：`Vector3`, `JobSystem` |
| 函数/变量 | snake_case：`get_center()`, `block_size_` |
| 私有成员 | 尾部下划线：`offset_`, `free_head_` |
| 常量 | `UPPER_SNAKE`：`INVALID_HANDLE`, `PI` |
| 伞形头文件 | 每个子系统一个：`math.h`, `allocator.h`, `event.h`, `task.h`, `graphics.h` |
| 标记属性 | 返回值方法用 `[[nodiscard]]`，常量表达式用 `constexpr` |
| 不可复制类型 | `= delete` 复制构造/赋值 |

---

## 15. 后续工作

> 随实现推进更新。

### 已完成

- [x] `core/log/` — spdlog 日志封装
- [x] `core/math/` — Vector2/3/4、Mat4、Quat、AABB、Transform3D
- [x] `core/allocator/` — LinearAllocator、PoolAllocator、ArenaAllocator
- [x] `core/event/` — Delegate、Signal、EventBus
- [x] `core/task/` — JobSystem（jthread + work-stealing）
- [x] `core/object/` — Object、ClassDB、RefCounted、ROVER_CLASS 宏
- [x] `core/graphics/` — GraphicsDevice 纯虚接口
- [x] **测试**：`tests/core/{math,allocator,event,object,task}_test.cpp`，75 个测试用例 / 255 个断言全绿；通过 3 次故意 bug 注入验证测试有效性（Vector3 cross 反向、PoolAllocator 计数器漏减、JobSystem worker 跳过任务执行）

### 已知限制

- **`LinearAllocator` 对齐上限 = `alignof(std::max_align_t)`**：底层 `make_unique<u8[]>` 仅保证此对齐。请求更大对齐时不会触发崩溃，但首次分配可能落在比要求弱的边界上。需要更高对齐时应使用 `ArenaAllocator`（chunk 内部对齐由实现承担）或后续增强 `LinearAllocator` 让其 over-allocate 缓冲区。
- **`JobSystem` 的 jobs-in-flight 计数器是计数语义**：当前在 `submit()` 增、worker 完成时减。若任务在 `submit()` 后但在 worker 拾取前出现计数错位，`wait_all()` 可能死循环（已用单元测试覆盖典型路径）。

### 待做

- [ ] `core/object/` — 属性绑定系统（`ClassDB::add_property`）
- [ ] `core/object/` — 方法绑定系统（`MethodBind`）
- [ ] `core/object/` — StringName 优化为 interned string
- [ ] `core/variant/` — Variant 类型系统（编辑器和脚本绑定需要）
- [ ] `core/time/` — Clock、Timer、delta time 封装
- [ ] `core/math/` — Basis（3×3 矩阵）、Plane、Ray、Frustum
- [ ] `core/math/` — 噪声函数（Perlin/Simplex，程序化生成需要）
- [ ] `core/allocator/` — StackAllocator（LIFO 模式）
- [ ] `core/allocator/` — `LinearAllocator` 的 over-aligned 模式
- [ ] `core/event/` — 线程安全版 Signal/EventBus
- [ ] `core/task/` — TaskGraph（依赖图调度）
- [ ] `core/graphics/` — Compute pipeline 支持
- [ ] `core/graphics/` — 资源描述符集（Descriptor Set / Bind Group）抽象

---

*Rover Engine Core v0.1.0 — 文档版本与实现同步。*
