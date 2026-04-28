# ADR-0003: scene 作为模块而非固定层

- **Status**: Accepted
- **Date**: 2026-04-26
- **Deciders**: core team

## Context

Godot 的 `scene/` 是顶级目录，`Node` 体系紧耦合在引擎核心。结果：

- 想要替换为 ECS 几乎不可能（`Node` 是所有其他系统的中心）
- 体素世界 / procedural 世界等不同范式难以共存
- 编辑器假设 scene 树存在，把它从核心拔出来风险极高

Rover 倾向 ECS（用 EnTT），同时希望保留传统场景树的能力。两者不应互斥。

## Decision

把 `scene/` 放进 **`modules/scene/`**，作为 **可插拔模块**：

- 与 `animation`、`particle`、`ui`、`ai`、`serialization` 平级
- 通过 CMake 选项 `ROVER_MODULE_SCENE` 控制是否启用
- 引擎核心不假设场景的存在，只通过事件总线 + 服务接口与场景交互

模块系统的抽象在 `modules/CMakeLists.txt` 中通过 `rover_collect_modules()` + `rover_generate_module_registration()` 实现，新增模块零样板代码。

## Consequences

**Positive:**

- ECS 与场景树可以共存（同一个 EnTT 世界，不同的查询 / 系统组）
- 体素 / procedural / 数据流等其他范式可以作为新模块共存（`modules/voxel/`、`modules/dataflow/`）
- 服务器项目（无图形）可以禁用 scene 模块编译，二进制尺寸更小
- 模块依赖图清晰，互相 include 受 CMake 约束

**Negative / Trade-offs:**

- 与 Godot / Unity 的「场景树是引擎中心」直觉不同，新贡献者需要适应
- 跨模块通信只能通过 services + EventBus，不像直接 include 那样方便
- 编辑器对场景的可视化是 `editor/` 的职责，不能假设 scene 存在（需运行时探测）

## Alternatives Considered

- **scene 作为顶级层（Godot 风格）**：复用度低，灵活性差，强行引入。放弃。
- **scene 作为 service**：service 是单例 + 重型系统语义，scene 应该可以多实例（多关卡、子场景）。语义错配。放弃。

## References

- [`docs/dev/ARCHITECTURE.md`](../../dev/ARCHITECTURE.md) §3 目录结构
- [EnTT](https://github.com/skypjack/entt)
- 未来：`docs/dev/MODULES.md`（待 modules/ 实现后补写）
