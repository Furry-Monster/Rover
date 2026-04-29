# Rover 引擎文档总览

> 本目录是 Rover 引擎所有非源码文档的根。每份文档都属于以下 **三类** 之一，请按需查阅。

---

## 我应该读哪一份？

| 我想…… | 去 |
|--------|----|
| 了解引擎当前实现长什么样 | [`dev/`](dev/) |
| 了解引擎目标、规划与历史决策 | [`product/`](product/) |
| 知道应该怎么写代码（命名、风格、约束） | [`standard/`](standard/) |
| 第一次接触引擎，不知道从哪开始 | 先 [`dev/ARCHITECTURE.md`](dev/ARCHITECTURE.md)，再 [`product/ROADMAP.md`](product/ROADMAP.md) |
| 查某个已记录渲染/引擎缺陷的调查结论 | [`debug/`](debug/) |
| 提交 PR 前自查 | [`standard/CODE_STYLE.md`](standard/CODE_STYLE.md) + [`standard/ARCHITECTURE_RULES.md`](standard/ARCHITECTURE_RULES.md) + [`standard/TESTING.md`](standard/TESTING.md) |
| 新增一个渲染后端 / 平台 / 模块 | [`product/ROADMAP.md`](product/ROADMAP.md) 看支持矩阵 → [`dev/`](dev/) 看现有实现模板 → [`product/adr/`](product/adr/) 写决策 |

---

## 三类文档的边界

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                   │
│  product/   做什么（What & Why）                                   │
│  ├─ ROADMAP        分阶段目标与支持矩阵                            │
│  ├─ REQUIREMENTS   功能/非功能需求清单                             │
│  └─ adr/           架构决策记录（每个决策一份）                     │
│                                                                   │
│  standard/  怎么做（Rules & Conventions）                          │
│  ├─ NAMING         命名约定                                        │
│  ├─ CODE_STYLE     代码风格                                        │
│  ├─ ARCHITECTURE_RULES   层依赖、include 约束                      │
│  ├─ DEBUGGING      调试与异常处理规范                              │
│  ├─ ASSETS         资源工作流                                      │
│  ├─ DEPENDENCIES   vendor 与模块依赖                               │
│  └─ TESTING        测试规范                                        │
│                                                                   │
│  dev/       现在长什么样（Current Implementation）                  │
│  ├─ ARCHITECTURE   层级结构、CMake target、注册系统快照             │
│  ├─ CORE           Layer 1 各子系统 API                            │
│  ├─ PLATFORM       Layer 2 平台实现快照                            │
│  ├─ DRIVERS        Layer 2 驱动实现快照                            │
│  └─ MISC           工程工具（rover-cli、CMake 模块）                │
│                                                                   │
│  debug/     已调查缺陷笔记（Incident notes，非强制规范）            │
│  └─ README + 个案 md（透视、驱动踩坑等）                           │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

**目录之间的引用方向**：

- `product/adr/` 的决策被 `standard/` 写成强制规则，最终落实为 `dev/` 的实现状态。
- `dev/` 描述事实，不引申意见；意见放在 `product/`，规则放在 `standard/`。
- `debug/` 记录已调查的缺陷与规避方式，**不**作为规范；必要时与 `dev/`、源码注释交叉引用。
- 出现冲突时，`product/` 是 source of truth：先改 ADR，再改 standard，再改 dev。

---

## 更新触发器

| 改动 | 必须同步 |
|------|---------|
| 新增 / 删除 / 重命名源文件 | 对应的 `dev/<area>.md` |
| 新增渲染后端 / 平台 | `dev/DRIVERS.md` 或 `dev/PLATFORM.md` + `product/ROADMAP.md` 支持矩阵 + 新 ADR |
| 跨层结构调整（如新增 Layer，调整依赖关系） | `dev/ARCHITECTURE.md` + `standard/ARCHITECTURE_RULES.md` + 新 ADR |
| 调整命名/风格规则 | `standard/NAMING.md` 或 `CODE_STYLE.md`，并把变更原因记录为 ADR |
| 新增需求 | `product/REQUIREMENTS.md` 添加新条目（编号 + 状态） |
| 新增第三方库 | `standard/DEPENDENCIES.md` + 解释引入理由的 ADR |
| 新增已结案缺陷笔记 | `debug/README.md` 索引表 + 个案 `debug/*.md` |

`.cursor/rules/` 已配置硬性提示，会在你触碰相关源代码时强制提醒同步对应文档。

---

## 命名约定

- 文件名：`UPPERCASE.md`（如 `NAMING.md`、`ROADMAP.md`）
- 目录名：lowercase（如 `dev/`、`product/`、`standard/`、`adr/`）
- ADR 文件：`ADR-NNNN-kebab-case-slug.md`（4 位编号）

---

## 子目录索引

- [`dev/`](dev/) — 当前实现快照
  - [`ARCHITECTURE.md`](dev/ARCHITECTURE.md) | [`CORE.md`](dev/CORE.md) | [`PLATFORM.md`](dev/PLATFORM.md) | [`DRIVERS.md`](dev/DRIVERS.md) | [`MISC.md`](dev/MISC.md)
- [`product/`](product/) — 产品文档
  - [`ROADMAP.md`](product/ROADMAP.md) | [`REQUIREMENTS.md`](product/REQUIREMENTS.md) | [`adr/INDEX.md`](product/adr/INDEX.md)
- [`standard/`](standard/) — 规范文档
  - [`NAMING.md`](standard/NAMING.md) | [`CODE_STYLE.md`](standard/CODE_STYLE.md) | [`ARCHITECTURE_RULES.md`](standard/ARCHITECTURE_RULES.md) | [`DEBUGGING.md`](standard/DEBUGGING.md) | [`ASSETS.md`](standard/ASSETS.md) | [`DEPENDENCIES.md`](standard/DEPENDENCIES.md) | [`TESTING.md`](standard/TESTING.md)
- [`debug/`](debug/) — 已结案缺陷与调查笔记（非规范）
  - [`README.md`](debug/README.md)
- [`superpowers/`](superpowers/) — 设计 spec 归档（每次重要重构产生一份）

---

*Rover Engine v0.1.0 — Phase 1 完成（基础架构 + 三角形渲染）。*
