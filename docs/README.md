# Rover 引擎文档

`docs/` 按实现、规划、规范和缺陷调查组织。初次阅读从 [架构](dev/ARCHITECTURE.md) 和 [路线图](product/ROADMAP.md) 开始；编写或修改文档时使用 [文档写作规范](standard/DOCUMENTATION.md)。

## 按任务查阅

| 任务 | 文档 |
|------|------|
| 理解层级、构建目标和注册顺序 | [ARCHITECTURE](dev/ARCHITECTURE.md) |
| 查询核心、平台和驱动 API | [CORE](dev/CORE.md)、[PLATFORM](dev/PLATFORM.md)、[DRIVERS](dev/DRIVERS.md) |
| 加载场景与运行示例 | [SCENE_LOADING](dev/SCENE_LOADING.md)、[内置场景](../main/builtin_assets/scenes/demos/README.md) |
| 声明渲染依赖、后处理与资源同步 | [FRAME_GRAPH](dev/FRAME_GRAPH.md)、[TRANSIENT_RESOURCES](dev/TRANSIENT_RESOURCES.md)、[POST_PROCESS](dev/POST_PROCESS.md) |
| 查询 Phase 2 剩余工作与验收证据 | [PHASE2_ACCEPTANCE](dev/PHASE2_ACCEPTANCE.md) |
| 使用构建、运行和调试工具 | [MISC](dev/MISC.md) |
| 查询目标、需求和设计依据 | [ROADMAP](product/ROADMAP.md)、[REQUIREMENTS](product/REQUIREMENTS.md)、[ADR 索引](product/adr/INDEX.md) |
| 编写代码与测试 | [命名](standard/NAMING.md)、[代码风格](standard/CODE_STYLE.md)、[架构约束](standard/ARCHITECTURE_RULES.md)、[测试](standard/TESTING.md) |
| 管理错误、资源和依赖 | [调试](standard/DEBUGGING.md)、[资源](standard/ASSETS.md)、[依赖](standard/DEPENDENCIES.md) |
| 查询已调查的缺陷 | [Debug 索引](debug/README.md) |

## 目录职责

| 目录 | 内容 | 维护方式 |
|------|------|----------|
| `dev/` | 当前实现、API、操作步骤和验证入口 | 随实现更新，计划单列为后续工作 |
| `product/` | 版本目标、需求状态和架构决策 | 决策写入 ADR，需求保留稳定编号 |
| `standard/` | 命名、风格、依赖、测试和文档规则 | 说明要求、适用条件与检查方法 |
| `debug/` | 缺陷现象、调查证据、修复和回归记录 | 按事件归档，标明日期与状态 |

架构决策以已接受的 ADR 为依据，规范定义执行要求，`dev/` 记录落地情况。发现差异时核对代码与 ADR；设计变更先记录决策，再同步规范和实现说明。历史 ADR 和缺陷记录保留当时的事实，后续进展通过带日期的更新或新 ADR 关联。

## 更新对应文档

| 改动 | 同步内容 |
|------|----------|
| 新增、删除或重命名源文件 | 对应 `dev/<area>.md` 的文件清单与接口说明 |
| 新增渲染后端或平台 | `dev/DRIVERS.md` 或 `dev/PLATFORM.md`、路线图支持矩阵、新 ADR |
| 跨层结构调整 | `dev/ARCHITECTURE.md`、`standard/ARCHITECTURE_RULES.md`、新 ADR |
| 调整命名或风格规则 | 对应规范及说明变更原因的 ADR |
| 新增或变更需求 | `product/REQUIREMENTS.md` 的编号、状态和目标版本 |
| 引入第三方库 | 依赖清单、集成步骤及说明引入理由的 ADR |
| 新增缺陷调查 | `debug/README.md` 索引与个案记录 |

文档目录使用小写名称；普通文档使用 `UPPERCASE.md`，ADR 使用 `ADR-NNNN-kebab-case-slug.md`。目录内链接使用相对路径，命令注明工作目录和前置条件。
