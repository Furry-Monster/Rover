# Rover 文档体系重组设计

- Date: 2026-04-28
- Status: Approved (in chat, before implementation)
- Author: assistant + user

## 1. 目标

引擎已完成 Phase 1（基础架构 + 三角形渲染）。现有 `docs/*.md` 全部偏向「当前实现快照」，缺少前瞻规划、设计决策记录与开发标准。重组目的：

1. 明确三类文档边界，避免相互重叠：
   - **product/** —— 我们要做什么（roadmap、需求、决策）
   - **standard/** —— 必须怎么做（规范、约束）
   - **dev/** —— 当前长什么样（实现快照）
2. 建立可演进的需求列表与 ADR 体系，承载未来跨 API、跨平台的决策。
3. 把现有 5 份「随实现演进」的文档迁入 `dev/` 子目录，配套更新 cursor rules。

## 2. 命名约定

- 文件名：`UPPERCASE.md`（保持现有规则）
- 目录名：全 lowercase（`dev/`、`product/`、`standard/`、`adr/`）
- ADR 文件：`ADR-NNNN-kebab-case-slug.md`（4 位编号，便于稳定排序）

## 3. 最终目录结构

```
docs/
├── README.md                                  顶层导航
│
├── product/                                   产品文档
│   ├── ROADMAP.md
│   ├── REQUIREMENTS.md
│   └── adr/
│       ├── INDEX.md
│       ├── ADR-0001-three-layer-architecture.md
│       ├── ADR-0002-cmake-as-single-build-system.md
│       ├── ADR-0003-modules-over-fixed-scene-tree.md
│       ├── ADR-0004-services-vs-servers-naming.md
│       ├── ADR-0005-cli-under-editor.md
│       └── ADR-0006-io-split-platform-and-serialization.md
│
├── standard/                                  规范文档
│   ├── NAMING.md
│   ├── CODE_STYLE.md
│   ├── ARCHITECTURE_RULES.md
│   ├── DEBUGGING.md
│   ├── ASSETS.md
│   ├── DEPENDENCIES.md
│   └── TESTING.md
│
└── dev/                                       开发快照
    ├── ARCHITECTURE.md       (从 docs/ 迁移)
    ├── CORE.md               (从 docs/ 迁移)
    ├── PLATFORM.md           (从 docs/ 迁移)
    ├── DRIVERS.md            (从 docs/ 迁移)
    └── MISC.md               (从 docs/ 迁移)
```

## 4. 三类文档的内容契约

### product/

| 文件 | 内容 | 何时更新 |
|------|------|---------|
| `ROADMAP.md` | 分阶段路线图；含 **渲染API × 平台 × 子系统** 支持矩阵；明确 v0.1 → v1.0 的能力门槛 | 每个 phase 结束时 |
| `REQUIREMENTS.md` | 功能需求 `[F-NNN]` + 非功能需求 `[NF-NNN]`，每条带状态标签（Proposed / Accepted / Implemented / Deprecated） | 任何需求被讨论或确认时 |
| `adr/INDEX.md` | ADR 列表 + 状态摘要 | 新增/废弃 ADR 时 |
| `adr/ADR-NNNN-*.md` | 单个决策。模板：Context / Decision / Consequences / Alternatives | 决策发生时新建；既有决策被推翻时新增 ADR 标记原 ADR 为 Superseded |

### standard/

| 文件 | 内容 |
|------|------|
| `NAMING.md` | 命名空间、类型、函数、变量、常量、文件、宏、注册函数命名规则 |
| `CODE_STYLE.md` | clang-format 详解、include 顺序、RAII、智能指针、错误处理、注释 |
| `ARCHITECTURE_RULES.md` | 层依赖矩阵、include 规则、注册顺序、抽象接口契约、严禁项 |
| `DEBUGGING.md` | Vulkan 验证层、ASan/UBSan/TSan、日志规范、断言策略、调试开关 |
| `ASSETS.md` | shader/texture/model/audio 的源格式、中间格式、运行时格式、导入导出工作流 |
| `DEPENDENCIES.md` | vendor 政策、CMake target 关系图、模块依赖图、第三方升级流程 |
| `TESTING.md` | doctest 模式、单元 vs 集成 vs 端到端、Mock 策略、CI 集成 |

### dev/

迁移后内容不变，仅修复内部相对链接（`../foo` → `../../foo`）。

## 5. ADR 模板

```markdown
# ADR-NNNN: <Title>

- Status: Accepted | Proposed | Superseded by ADR-XXXX | Deprecated
- Date: YYYY-MM-DD
- Deciders: <name(s) or "core team">

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
```

初始 6 条 ADR 从现有 `ARCHITECTURE.md §12 FAQ` 抽取。`dev/ARCHITECTURE.md` §12 替换为 `[详见 product/adr/](../product/adr/INDEX.md)` 指针。

## 6. Cursor rules 改动

### 现有 4 条改 path

| 文件 | 旧引用 | 新引用 |
|------|-------|-------|
| `core-doc-sync.mdc` | `docs/CORE.md` | `docs/dev/CORE.md` |
| `drivers-doc-sync.mdc` | `docs/DRIVERS.md` | `docs/dev/DRIVERS.md` |
| `platform-doc-sync.mdc` | `docs/PLATFORM.md` | `docs/dev/PLATFORM.md` |
| `misc-doc-sync.mdc` | `docs/MISC.md` | `docs/dev/MISC.md` |

### 新增 3 条

| 文件 | 触发 glob | 作用 |
|------|----------|------|
| `architecture-doc-sync.mdc` | `**/CMakeLists.txt`, `*/register_*_types.{h,cpp}` | 强制更新 `docs/dev/ARCHITECTURE.md` |
| `standard-doc-trigger.mdc` | `{core,drivers,platform,services,modules,editor,main,tests}/**/*.{h,cpp}` | 提示先读 `docs/standard/NAMING.md` 与 `CODE_STYLE.md` |
| `adr-on-decision.mdc` | `**/CMakeLists.txt`, `core/**/*.h` | 提示跨层结构变更时新增 ADR |

## 7. 链接修复（机械改动）

`dev/ARCHITECTURE.md` 中所有 `../<path>` 链接需改为 `../../<path>`（迁移后 `dev/` 多了一级深度）。

## 8. 实施顺序

1. 创建目录：`docs/dev/`、`docs/product/adr/`、`docs/standard/`
2. 移动现有 5 份 `.md` → `docs/dev/`，修复 `ARCHITECTURE.md` 中 `../` 链接为 `../../`
3. 写 `docs/README.md`（顶层导航 + 三类文档定位）
4. 更新 4 条现有 cursor rules
5. 写 6 条 ADR + `adr/INDEX.md`，把 `dev/ARCHITECTURE.md` §12 替换为指针
6. 写 `product/ROADMAP.md`、`product/REQUIREMENTS.md`
7. 写 `standard/` 7 份文件
8. 新增 3 条 cursor rules
9. 验证：`git status`、grep 残余旧路径、`ReadLints`

## 9. 验证准则

- 所有原 `docs/*.md` 已移入 `dev/`，根 `docs/` 仅剩 `README.md` + 三个子目录 + `superpowers/`
- `grep -r "docs/CORE.md\|docs/PLATFORM.md\|docs/DRIVERS.md\|docs/MISC.md" .cursor/ docs/ 2>/dev/null` 无剩余命中
- `dev/ARCHITECTURE.md` 中所有 `[link](../...)` 解析成功（路径存在）
- `product/adr/INDEX.md` 列出 6 条 ADR 且状态正确
- 每份 `standard/*.md` 至少含：目标、范围、规则正文、违规反例、引用
- 顶层 `docs/README.md` 提供清晰的「我该读哪份文档」导航

## 10. 范围外（明确不做）

- 不重写引擎源代码
- 不创建运行时新功能
- 不引入文档生成工具（如 sphinx）
- 不写英文版（保持中文，与现有文档一致）
