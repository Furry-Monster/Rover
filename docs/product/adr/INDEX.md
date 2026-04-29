# Architecture Decision Records — Index

> 每条架构级决策都在此目录下沉淀为一份独立 ADR。新增决策 = 新增文件，旧决策被推翻 = 新增 ADR 并把旧的状态改为 `Superseded`。

---

## 状态总览

| 编号 | 标题 | 状态 | 日期 |
|------|-----|------|-----|
| [ADR-0001](ADR-0001-three-layer-architecture.md) | core/services/drivers 三层倒置 | Accepted | 2026-04-26 |
| [ADR-0002](ADR-0002-cmake-as-single-build-system.md) | 单一 CMake 构建系统 | Accepted | 2026-04-26 |
| [ADR-0003](ADR-0003-modules-over-fixed-scene-tree.md) | scene 作为模块而非固定层 | Accepted | 2026-04-26 |
| [ADR-0004](ADR-0004-services-vs-servers-naming.md) | 用 services 而不是 servers | Accepted | 2026-04-26 |
| [ADR-0005](ADR-0005-cli-under-editor.md) | CLI 编辑器接口归属 editor/ | Accepted | 2026-04-26 |
| [ADR-0006](ADR-0006-io-split-platform-and-serialization.md) | IO 拆分到 platform/ 与 modules/serialization/ | Accepted | 2026-04-26 |
| [ADR-0007](ADR-0007-shader-source-language.md) | Shader 源语言选 Slang / HLSL + 转译器架构 | Accepted | 2026-04-29 |
| [ADR-0008](ADR-0008-vendor-manual-fetch.md) | vendor 第三方依赖统一手动下载（禁用 submodule / FetchContent / find_package） | Accepted | 2026-04-29 |
| [ADR-0009](ADR-0009-variant-implementation-strategy.md) | Variant 实现策略：手写 union + tag + 64B inline 缓冲区 | Accepted | 2026-04-29 |
| [ADR-0010](ADR-0010-graphics-bind-group-design.md) | GraphicsDevice 描述符绑定：WebGPU 风格 BindGroup | Accepted | 2026-04-29 |

---

## 模板

新建 ADR 时复制此模板：

```markdown
# ADR-NNNN: <Title>

- **Status**: Proposed | Accepted | Superseded by ADR-XXXX | Deprecated
- **Date**: YYYY-MM-DD
- **Deciders**: <name(s) or "core team">

## Context
（背景 / 痛点 / 触发因素）

## Decision
（最终选择，单段叙述）

## Consequences

**Positive:**
- ...

**Negative / Trade-offs:**
- ...

## Alternatives Considered
- 选项 A：放弃理由
- 选项 B：放弃理由

## References
- 相关代码 / 文档 / 外部链接
```

---

## 何时新建 ADR

任意一项命中即应该写 ADR：

- 跨层结构调整（新增 Layer、调整依赖方向）
- 新增渲染后端 / 平台 / 重型依赖（影响构建拓扑）
- 接口契约变化（如修改 `GraphicsDevice` 公共 API）
- 命名 / 编码规范的根本性调整
- 推翻既有 ADR

---

*Rover Engine ADR Registry — 与项目同步更新。*
