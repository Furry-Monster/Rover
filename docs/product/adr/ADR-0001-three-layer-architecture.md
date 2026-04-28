# ADR-0001: core/services/drivers 三层倒置

- **Status**: Accepted
- **Date**: 2026-04-26
- **Deciders**: core team

## Context

引擎需要支持多个渲染后端（Vulkan / D3D12 / Metal / 未来的 WebGPU）和多个平台（Linux / Windows / macOS / Android / iOS / Web）。如果上层代码（场景、动画、UI）直接依赖具体后端，则：

- 替换或新增后端会污染整棵代码树
- 单元测试无法在没有 GPU 的环境中编译
- 跨平台移植成本随依赖蔓延而平方级增长

参考 Godot 的分层（core/servers/drivers）证明了「依赖倒置 + 抽象在底层声明」的可行性。

## Decision

采用 **三层倒置**：

1. **`core/`（Layer 1）** —— 声明所有跨后端 / 跨平台的抽象接口（如 `GraphicsDevice`、`WindowSystem`），不引用任何具体实现
2. **`drivers/`（Layer 2）** —— 实现 `core/` 中的抽象接口，每个后端一个目录（`drivers/vulkan/`、`drivers/d3d12/` 等）
3. **`services/`（Layer 3）** —— 仅依赖 `core/` 的抽象，不感知具体后端
4. **`main/`** —— 唯一进行具体绑定的位置（如 `get_vulkan_device()` → `GraphicsDevice*`）

服务层（services）调用驱动层（drivers）只能 **运行时** 通过抽象指针，编译期不可 include 任何 driver 头文件。

## Consequences

**Positive:**

- 上层代码（services / modules / editor）可以在不知道具体后端的情况下编译
- 替换后端只影响 `drivers/<api>/` 与 `main/`
- Mock driver 可以让 services 在 CI 中跑单元测试，无需 GPU
- 跨平台移植成本线性而非平方

**Negative / Trade-offs:**

- 抽象接口的设计需要前期投入，且必须能容纳所有目标后端的能力（最小公倍数 vs 最大公约数取舍）
- 抽象层带来一次虚函数调用开销（实测可忽略，但渲染 hot path 要避免在虚函数内做循环）
- 接口扩展（新增方法）会迫使所有后端同步实现

## Alternatives Considered

- **直接依赖（无抽象层）**：开发速度最快，但不可扩展。Phase 1 之后会越来越痛。放弃。
- **运行时多态 + 单元素后端注册**（类似 Unreal 的 `RHIModule`）：表达力更强，但实现复杂度高，初期不需要。可在未来 ADR 中升级。

## References

- [`docs/dev/ARCHITECTURE.md`](../../dev/ARCHITECTURE.md) §2、§7
- [`core/graphics/graphics_device.h`](../../../core/graphics/graphics_device.h)
- [`core/graphics/window_system.h`](../../../core/graphics/window_system.h)
- Godot 引擎架构文档：<https://docs.godotengine.org/en/stable/contributing/development/core_and_modules/index.html>
