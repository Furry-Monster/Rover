# ADR-0002: 单一 CMake 构建系统

- **Status**: Accepted
- **Date**: 2026-04-26
- **Deciders**: core team

## Context

游戏引擎的构建复杂度在多平台/多后端场景下会爆炸。社区里常见做法：

- Godot 用 SCons（历史选择）
- Unreal 用 UnrealBuildTool（C# 自研）
- Unity 用 Bee（Bee.Toolchain，C# 自研，因 Unity 大量 C# 代码而合理）
- bgfx 用 GENie（再生成 Makefile/Project）

引入第二套构建系统会带来：CI 配置、IDE 支持、贡献者 onboarding 三重负担。Rover 是纯 C++ 工程，没有 C# 用户层，理由不充分。

## Decision

**全工程统一使用 CMake + Ninja，禁止引入第二套构建系统**。

- 根 `CMakeLists.txt` 做总装
- `misc/cmake/` 提供 `Rover*` 系列模块（Options / Compiler / Utils / Shader）
- vendor 库要么是 CMake 原生，要么通过 `add_subdirectory` 包装
- `compile_commands.json` 自动同步到仓库根，给 clangd / IDE 用

附属决策：使用 **CMake Presets** 与 `rover-cli`（Python wrapper）作为面向用户的工程化入口，但底层仍是 CMake。

## Consequences

**Positive:**

- clangd / VS Code / CLion / Visual Studio 全部原生支持 CMake，IDE 体验一致
- 跨平台一致：Linux / Windows / macOS 用同一份 CMakeLists.txt
- CTest 集成测试无需额外配置
- 新贡献者 onboarding 成本低（CMake 是 C++ 圈事实标准）
- 构建参数全部可通过 `option(ROVER_*)` 暴露，可被 CI 或脚本覆盖

**Negative / Trade-offs:**

- CMake 语法不优雅，复杂逻辑（如模块自动注册生成）需要额外辅助函数
- 不像 Bazel 那样有强缓存与 hermetic 构建（接受）
- 与某些 vendor 库的非 CMake 构建脚本集成需要 wrapper（如 SDL3 自带 CMake，无问题）

## Alternatives Considered

- **SCons**：Godot 的选择，但 Python DSL 表达力虽强，IDE 集成弱，社区使用率下降中。放弃。
- **Bazel**：Hermetic + 强缓存优秀，但学习曲线陡，C++ 圈外少有贡献者熟悉，且与 vendor 第三方 CMake 库整合复杂。放弃。
- **Meson**：语法清晰，但生态比 CMake 小，IDE 支持弱于 CMake。放弃。
- **xmake**：国内活跃，但社区规模与文档成熟度仍不及 CMake。放弃。

## References

- [`docs/dev/MISC.md`](../../dev/MISC.md) §9 CMake 模块
- [`misc/cmake/`](../../../misc/cmake/)
- [Modern CMake](https://cliutils.gitlab.io/modern-cmake/)
