# ADR-0005: CLI 编辑器接口归属 editor/

- **Status**: Accepted
- **Date**: 2026-04-26
- **Deciders**: core team

## Context

引擎需要两类「编辑器入口」：

1. **GUI 编辑器**：人类用，ImGui 驱动，能看到场景、Inspector、Asset Browser
2. **CLI 编辑器**：AI Agent / 构建脚本 / CI 用，结构化命令（导入资源、批量构建、运行测试场景、编辑节点属性）

需要决定 CLI 接口放在哪：是引擎运行时的一部分（在 `main/` 解析 argv），还是编辑器的一部分？

注意：这里的 CLI **不是**引擎运行时启动参数（那部分自然在 `main/` 里）。而是 **编辑器命令行接口**，用来不开 GUI 也能驱动编辑器功能。

## Decision

CLI 编辑器归属 **`editor/cli/`**，与 `editor/gui/` 平级：

- 两者共享项目管理、Asset Pipeline、ClassDB 等基础设施
- `Rover::Editor` 聚合 target 同时链接 `rover_editor_gui` 和 `rover_editor_cli`
- 仅在 `ROVER_EDITOR=ON` 时编译
- AI Agent 通过 CLI 调用即可完成大部分非可视化编辑任务

GUI 与 CLI 各自有独立的 `register_editor_gui()` / `register_editor_cli()` 入口，由 `register_editor_types()` 聚合。

## Consequences

**Positive:**

- AI Agent 与 GUI 用户面对的是同一套编辑器能力（不会出现 CLI 缺某些 GUI 才有的操作的情况）
- 重构 GUI 不影响 CLI（共享层稳定）
- CI 中可以跑「编辑器自动化测试」（导入一批资产，跑 build，比对输出），不需要图形栈
- 给 AI 友好的结构化输出（JSON / 表格）天然容易实现

**Negative / Trade-offs:**

- CLI 不能在引擎运行时（无编辑器构建）使用——但这本来也不是它的目标场景
- 两个入口都依赖共享层，新增编辑器功能时要同时考虑暴露给 GUI 和 CLI
- 命令解析与帮助文档需要双重维护（GUI 有按钮，CLI 有 `--help`）

## Alternatives Considered

- **CLI 在 `main/` 里**：会把编辑器代码强行链接到运行时可执行文件，违反 `ROVER_EDITOR=OFF` 应该精简引擎的初衷。放弃。
- **CLI 作为独立可执行文件 `rover_cli`**：分发简单，但意味着 CLI 不能直接 link 到 GUI 共享对象，跨进程通信复杂度高。放弃。
- **CLI 直接复用 `misc/scripts/rover-cli`**：那是开发者工程脚本（configure/build/run），与编辑器命令是不同范畴。放弃。

## References

- [`docs/dev/ARCHITECTURE.md`](../../dev/ARCHITECTURE.md) §2 分层架构、§3 目录结构
- 未来：`docs/dev/EDITOR.md`（编辑器实现后补写）
