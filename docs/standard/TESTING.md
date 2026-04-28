# 测试规范

> 本规范定义 Rover 引擎使用 [doctest](https://github.com/doctest/doctest) 编写测试的约束与组织方式。

---

## 1. 框架与基础设施

- **框架**：[doctest](https://github.com/doctest/doctest) — header-only，编译快，C++20 友好
- **入口**：[`tests/main.cpp`](../../tests/main.cpp) — 仅含 `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`
- **运行**：
  - `./bin/<config>/rover_tests`
  - `./misc/scripts/rover-cli test`
  - `ctest --test-dir build/<config>`

---

## 2. 目录组织

测试文件镜像被测代码的目录结构：

```
tests/
├── main.cpp                       doctest 入口
├── core/
│   ├── allocator_test.cpp        测试 core/allocator/
│   ├── event_test.cpp             测试 core/event/
│   ├── math_test.cpp              测试 core/math/
│   ├── object_test.cpp            测试 core/object/
│   └── task_test.cpp              测试 core/task/
├── services/
│   └── graphics_test.cpp          测试 services/graphics/（通过 mock）
├── modules/
│   └── scene_test.cpp             测试 modules/scene/
└── drivers/
    └── vulkan_format_test.cpp     测试 drivers/vulkan/vk_format
```

- 一个被测目录对应一个或多个 `*_test.cpp` 文件
- 文件名 `<topic>_test.cpp`（topic 是被测主题，如 `math`、`event`）
- 不混搭多个无关主题在同一文件

---

## 3. 测试分类

### 3.1 单元测试（unit）

测试单个类 / 函数 / 模块的孤立行为。**不**依赖：
- GPU
- 文件系统（除非通过 RAII 临时目录）
- 网络
- 时钟（用 mock）

例：

```cpp
// tests/core/math_test.cpp
#include <doctest/doctest.h>
#include "core/math/vector3.h"

TEST_CASE("Vector3 dot product")
{
    rover::Vector3 a{1, 2, 3};
    rover::Vector3 b{4, 5, 6};
    CHECK(a.dot(b) == doctest::Approx(32.0));
}

TEST_CASE("Vector3 normalization")
{
    rover::Vector3 v{3, 0, 4};
    auto n = v.normalized();
    CHECK(n.length() == doctest::Approx(1.0));
}
```

### 3.2 集成测试（integration）

跨多个组件的协作测试。例：
- `services/graphics/` + Mock `GraphicsDevice`
- `modules/scene/` + `core/event/`
- `editor/cli/` + 真实 import 流程

```cpp
// tests/services/graphics_test.cpp
#include <doctest/doctest.h>
#include "services/graphics/graphics_service.h"
#include "tests/mocks/mock_graphics_device.h"

TEST_CASE("GraphicsService routes to device")
{
    rover::MockGraphicsDevice mock;
    rover::GraphicsService    svc;
    svc.init(&mock, nullptr);

    auto handle = svc.create_buffer(/*...*/);
    CHECK(mock.create_buffer_call_count == 1);
}
```

### 3.3 端到端测试（e2e，计划）

驱动真实 driver / platform 的测试。仅在 CI 的特定环境中跑（需要 GPU 或显示器）。

| 测试类型 | 当前状态 | 何时跑 |
|---------|---------|-------|
| Unit | ✅ 已就绪（75 cases） | 每次本地 build + CI |
| Integration | 🎯 Phase 2 起 | CI 全部 |
| E2E | 🎯 Phase 5 起 | 仅特定 CI 节点 |

---

## 4. doctest 模式

### 4.1 基本结构

```cpp
#include <doctest/doctest.h>
#include "<被测头文件>"

TEST_CASE("人类可读的描述")
{
    // arrange
    rover::Foo foo;

    // act
    auto result = foo.bar();

    // assert
    CHECK(result == 42);
}
```

### 4.2 SUBCASE 共享 setup

```cpp
TEST_CASE("LinearAllocator behavior")
{
    rover::LinearAllocator alloc(1024);   // setup（每个 SUBCASE 重新初始化）

    SUBCASE("allocates aligned blocks")
    {
        void* p = alloc.allocate(64, 16);
        CHECK(p != nullptr);
        CHECK(reinterpret_cast<uintptr_t>(p) % 16 == 0);
    }

    SUBCASE("reset reuses memory")
    {
        alloc.allocate(512);
        alloc.reset();
        CHECK(alloc.bytes_used() == 0);
    }
}
```

### 4.3 断言宏选择

| 宏 | 行为 |
|----|------|
| `CHECK(cond)` | 失败继续执行（推荐，多个失败一次报告） |
| `REQUIRE(cond)` | 失败立即终止当前 TEST_CASE（用于前提条件） |
| `CHECK_FALSE(cond)` | 检查表达式为 false |
| `CHECK_THROWS(expr)` | 检查抛异常（Rover 不用异常，所以罕用） |
| `CHECK_NOTHROW(expr)` | 检查不抛异常 |
| `CHECK(a == doctest::Approx(b))` | 浮点近似比较 |

**默认用 `CHECK`**，仅当后续断言依赖前提时用 `REQUIRE`：

```cpp
TEST_CASE("Buffer create returns valid handle")
{
    auto handle = device.create_buffer(desc);
    REQUIRE(handle != INVALID_HANDLE);   // 前提：handle 必须有效
    
    auto* res = device.get_buffer(handle);
    CHECK(res != nullptr);
    CHECK(res->size == desc.size);
}
```

---

## 5. 测试命名

- TEST_CASE 描述使用**人类可读句子**，不写代码风格名
- 描述应当回答："这个测试在验证什么？"

```cpp
// ✅
TEST_CASE("Vector3 normalization preserves direction")
TEST_CASE("EventBus delivers events in publish order")
TEST_CASE("LinearAllocator returns nullptr when out of space")

// ❌
TEST_CASE("vector3_test_1")
TEST_CASE("test_normalize")
```

---

## 6. Mock 与 Fake

### 6.1 抽象接口的 mock

`GraphicsDevice` / `WindowSystem` / `FileSystem` 等抽象接口都有 mock 实现，放在 `tests/mocks/`：

```cpp
// tests/mocks/mock_graphics_device.h
#pragma once
#include "core/graphics/graphics_device.h"

namespace rover {

class MockGraphicsDevice : public GraphicsDevice
{
public:
    bool init(WindowSystem& window) override
    {
        init_call_count++;
        return true;
    }
    // ... 其他纯虚方法的简单实现 + 调用计数

    u32 init_call_count            = 0;
    u32 create_buffer_call_count   = 0;
    // ...
};

} // namespace rover
```

### 6.2 何时用 mock vs 真实实现

| 情况 | 推荐 |
|------|------|
| 测试纯逻辑（不依赖 GPU/IO） | 真实实现（如直接用 `LinearAllocator`） |
| 测试 services（需要 GraphicsDevice） | Mock |
| 测试 driver 自身 | 真实 GPU（仅在 e2e CI 节点） |
| 测试事件路由 | 真实 EventBus |

---

## 7. 测试覆盖要求

### 7.1 强制覆盖

PR 引入新代码时**必须**有对应测试，下列情况除外：

- 仅 GPU 操作（待 e2e 框架）
- 仅平台特定 API（无法在 CI 中复现）
- 纯绑定 / 透传代码（如 register_*_types 函数）

### 7.2 优先级

| 子系统 | 测试优先级 |
|--------|-----------|
| `core/` 全部 | 🔴 最高（单元覆盖） |
| `services/` 路由逻辑 | 🟠 高（mock 集成） |
| `modules/` 业务逻辑 | 🟠 高 |
| `drivers/` GPU 路径 | 🟡 中（e2e） |
| `platform/` OS 适配 | 🟡 中（按平台） |
| `editor/` UI | 🟢 低（可视化测试） |

### 7.3 当前测试覆盖（v0.1）

- ✅ `tests/core/allocator_test.cpp` — 全部分配器
- ✅ `tests/core/event_test.cpp` — Delegate / Signal / EventBus
- ✅ `tests/core/math_test.cpp` — Vector / Mat4 / Quat / AABB / Transform
- ✅ `tests/core/object_test.cpp` — Object / ClassDB / RefCounted
- ✅ `tests/core/task_test.cpp` — JobSystem / WorkStealingQueue
- 🎯 待补：services / drivers / modules / platform 覆盖

---

## 8. 性能 / 基准测试（计划）

不放在 `tests/` 内（doctest 不是 benchmark 框架）。计划：

- 在 `tests/bench/` 单独放 benchmark
- 候选框架：Google Benchmark / nanobench
- `rover bench` 子命令（[F-607](../product/REQUIREMENTS.md)）

---

## 9. CI 集成

### 9.1 当前

```bash
./misc/scripts/rover-cli test                  # 本地
ctest --test-dir build/debug --output-on-failure   # CTest
```

### 9.2 GitHub Actions（计划）

```yaml
matrix:
  os: [ubuntu-latest, windows-latest, macos-latest]
  build_type: [Debug, Release]

steps:
  - configure
  - build
  - test                    # rover_tests，必须全绿
  - lint                    # clang-tidy（计划）
```

PR 必须全平台 + 全配置全绿才能合入。

---

## 10. 反例

```cpp
// ❌ 测试依赖外部状态
TEST_CASE("Reads config file")
{
    auto cfg = read_config("/etc/rover.conf");   // 测试在 CI 中找不到此文件
    CHECK(cfg.valid);
}

// ✅ 改为接收路径或注入 FileSystem mock
TEST_CASE("Reads config file")
{
    rover::TmpDir dir;
    dir.write("rover.conf", "key=value\n");
    auto cfg = read_config(dir.path() / "rover.conf");
    CHECK(cfg.valid);
}

// ❌ 测试名晦涩
TEST_CASE("test1") { ... }
TEST_CASE("foobar") { ... }

// ❌ 一个 TEST_CASE 测多个不相关行为
TEST_CASE("everything")
{
    CHECK(vector_dot_works());
    CHECK(allocator_works());
    CHECK(event_bus_works());
    // 失败时无法定位是哪个组件
}

// ❌ 用 sleep 等待异步事件
TEST_CASE("Async job completes")
{
    job_system.submit(...);
    std::this_thread::sleep_for(100ms);   // flaky！
    CHECK(done);
}

// ✅ 使用同步原语等待
TEST_CASE("Async job completes")
{
    auto handle = job_system.submit(...);
    handle.wait();
    CHECK(done);
}
```

---

## 11. 参考

- [doctest 文档](https://github.com/doctest/doctest/blob/master/doc/markdown/readme.md)
- [`tests/main.cpp`](../../tests/main.cpp)
- [`docs/dev/CORE.md`](../dev/CORE.md) — 被测系统的设计
- [`docs/dev/MISC.md`](../dev/MISC.md) §5.6 `rover test` 子命令
- [`docs/product/REQUIREMENTS.md`](../product/REQUIREMENTS.md) §2.4 可测试性需求
