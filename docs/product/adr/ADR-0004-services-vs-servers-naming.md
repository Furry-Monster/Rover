# ADR-0004: 用 services 而不是 servers

- **Status**: Accepted
- **Date**: 2026-04-26
- **Deciders**: core team

## Context

Godot 把渲染、物理、音频等重型单例叫做 `servers/`（如 `RenderingServer`、`PhysicsServer`），这个命名反映了它们对外提供 RPC 风格调用的语义。

但在游戏引擎语境下：

- "Server" 更常被理解为网络服务器（multiplayer / online server / dedicated server）
- 中文/英文双语场景下「服务器」歧义更严重
- service-oriented architecture (SOA) 在工业界已经是更广为人知的术语

新引擎没有历史包袱，可以选择更精确的命名。

## Decision

使用 **`services/`** 命名顶级单例系统目录，对应类命名为 `<Name>Service`：

- `services/graphics/` → `GraphicsService`
- `services/physics/` → `PhysicsService`
- `services/audio/` → `AudioService`
- `services/network/` → `NetworkService`

未来的网络服务器实现归 `services/network/` 里的某个 `NetworkServer` 类，与 `NetworkService` 区分（service 是面向引擎的能力提供者，server 是其内部的具体网络端点实现）。

## Consequences

**Positive:**

- 命名歧义最小：service 提供能力，server 是网络端点
- 与 SOA / DI 概念对齐，新贡献者更容易理解
- 中英双语文档下歧义减少

**Negative / Trade-offs:**

- 与 Godot 的命名习惯不一致，从 Godot 迁移过来的贡献者要适应
- 极少数文章直接照搬 Godot 术语会需要翻译

## Alternatives Considered

- **使用 `servers/`**：与 Godot 一致，但歧义大。放弃。
- **使用 `engine/`**：太空泛，无法表达「单例 + 重型」语义。放弃。
- **使用 `subsystems/`**：表达力够，但更长且与 Unreal 的 `USubsystem` 概念混淆。放弃。

## References

- [`docs/dev/ARCHITECTURE.md`](../../dev/ARCHITECTURE.md) §2.1 各层职责速查表
