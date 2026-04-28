# Rover 引擎 Misc Tooling 文档

> **权威文档** — 对 `misc/` 下任何工具脚本的修改都 **必须** 同步更新本文档。修改前请先阅读本文档了解现有设计。

---

## 目录

1. [概述](#1-概述)
2. [文件清单](#2-文件清单)
3. [快速开始](#3-快速开始)
4. [整体架构](#4-整体架构)
5. [子命令参考](#5-子命令参考)
   - [configure](#51-rover-configure)
   - [build](#52-rover-build)
   - [clean](#53-rover-clean)
   - [run](#54-rover-run)
   - [debug](#55-rover-debug)
   - [test](#56-rover-test)
   - [format](#57-rover-format)
   - [shaders](#58-rover-shaders)
   - [status](#59-rover-status)
6. [核心模块](#6-核心模块)
7. [扩展指南：添加新子命令](#7-扩展指南添加新子命令)
8. [设计原则](#8-设计原则)
9. [CMake 模块（misc/cmake/）](#9-cmake-模块misccmake)
10. [后续工作](#10-后续工作)

---

## 1. 概述

`misc/` 包含两类支持工程化所需的工具：

| 子目录 | 内容 |
|--------|------|
| `misc/cmake/` | 项目级 CMake 模块（构建选项、编译标志、辅助函数、shader 编译） |
| `misc/scripts/` | Python 开发者 CLI（`rover-cli`），统一封装 cmake / ninja / gdb / clang-format 等日常工作流 |

这是一个 **可维护的工程工具集**，不是一次性的脚本拼凑。`misc/scripts/` 是结构化的 Python 包：

- 单一入口（`rover-cli` 可执行文件）+ 子命令模式
- 共享基础设施（路径发现、日志、子进程、Vulkan SDK 检测）
- 每个子命令一个 `.py` 文件，统一 4 元接口（`NAME` / `HELP` / `add_args(parser)` / `run(paths, build_type, args)`）
- 仅依赖 Python 标准库（≥ 3.10），无 pip 依赖

---

## 2. 文件清单

```
misc/
├── cmake/                              CMake 模块（被根 CMakeLists.txt 加载）
│   ├── RoverOptions.cmake              ROVER_* 编译选项与平台检测
│   ├── RoverCompiler.cmake             rover_compile_flags 接口库 + sanitizer
│   ├── RoverUtils.cmake                rover_add_library / rover_glob_sources / 模块注册生成
│   ├── RoverShader.cmake               rover_add_shader（GLSL → SPIR-V → C 头）
│   └── RoverVersion.h.in               生成 rover_version.h 的模板
│
└── scripts/
    ├── rover-cli                       可执行启动器（chmod +x，指向 rover/cli.py）
    └── rover/                          Python 包
        ├── __init__.py                 版本号
        ├── __main__.py                 允许 `python -m rover`
        ├── cli.py                      argparse 主入口 + 子命令分发
        ├── config.py                   ProjectPaths（自动定位仓库根） + BuildType
        ├── log.py                      ANSI 彩色日志（NO_COLOR / TTY 自适应）
        ├── shell.py                    subprocess 包装 + 工具发现
        ├── vulkan.py                   Vulkan SDK / 验证层路径自动发现
        └── commands/                   子命令模块（每命令一文件）
            ├── __init__.py             ALL_COMMANDS 注册表
            ├── configure.py
            ├── build.py
            ├── clean.py
            ├── run.py
            ├── debug.py
            ├── test.py
            ├── format.py
            ├── shaders.py
            └── status.py
```

共 **18 个 Python 文件**（约 1100 行）+ **5 个 CMake 模块**。

---

## 3. 快速开始

```bash
# 从仓库任何位置都可以调用：
./misc/scripts/rover-cli --help               # 查看所有子命令
./misc/scripts/rover-cli status                # 健康检查报告

# 第一次构建：
./misc/scripts/rover-cli configure              # 默认 Debug 配置
./misc/scripts/rover-cli build                  # 编译

# 日常迭代（build 自动调用 configure）：
./misc/scripts/rover-cli build                  # 增量编译
./misc/scripts/rover-cli run                    # 运行（自动编译）
./misc/scripts/rover-cli run --validation       # 启用 Vulkan 验证层
./misc/scripts/rover-cli debug --validation     # 在 gdb 下运行 + 验证

# Release 构建：
./misc/scripts/rover-cli -r build
./misc/scripts/rover-cli -r run

# 清理：
./misc/scripts/rover-cli clean                  # 仅 build/debug
./misc/scripts/rover-cli clean --all            # build/debug + bin/debug
./misc/scripts/rover-cli clean --everything     # 所有构建产物

# 测试 / 格式化 / shader：
./misc/scripts/rover-cli test
./misc/scripts/rover-cli format --check         # 检查（CI 用）
./misc/scripts/rover-cli format                 # 实际改写
./misc/scripts/rover-cli shaders                # 重编 shader
```

**建议**：把 `misc/scripts/` 加入 `PATH`，或在 shell rc 中添加 alias：

```bash
alias rv='./misc/scripts/rover-cli'             # 项目内
# 或全局（需要 cd 到 Rover 目录）：
alias rover-cli="$ROVER_ROOT/misc/scripts/rover-cli"
```

---

## 4. 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│ 用户终端                                                     │
└──┬──────────────────────────────────────────────────────────┘
   │  ./misc/scripts/rover-cli <command> [args...]
   ▼
┌─────────────────────────────────────────────────────────────┐
│ rover-cli  (executable launcher)                            │
│   - 检查 Python ≥ 3.10                                      │
│   - 添加 misc/scripts/ 到 sys.path                          │
│   - 调用 rover.cli.main()                                   │
└──┬──────────────────────────────────────────────────────────┘
   │
   ▼
┌─────────────────────────────────────────────────────────────┐
│ rover.cli                                                   │
│   - argparse 主解析器（global flags：-v / -d / -r）          │
│   - 子命令子解析器（每个子命令注册自身参数）                 │
│   - ProjectPaths.discover() 定位仓库根                      │
│   - 路由到 mod.run(paths, build_type, args)                 │
└──┬──────────────────────────────────────────────────────────┘
   │
   ▼
┌─────────────────────────────────────────────────────────────┐
│ rover.commands.<name>                                       │
│   - 每个子命令独立模块                                       │
│   - 调用 shell.run() 执行 cmake/ninja/gdb/clang-format      │
│   - 通过 log.* 报告进度                                      │
└──┬──────────────────────────────────────────────────────────┘
   │
   ▼
┌─────────────────────────────────────────────────────────────┐
│ 共享基础设施                                                 │
│   config.py   - 路径与构建类型                               │
│   log.py      - 彩色日志                                    │
│   shell.py    - subprocess 包装 + which/require              │
│   vulkan.py   - SDK 自动发现                                │
└─────────────────────────────────────────────────────────────┘
```

### 关键不变量

1. **从任何 cwd 都能正确工作**：`ProjectPaths.discover()` 向上查找包含 `project(RoverEngine` 的 `CMakeLists.txt`
2. **零 pip 依赖**：仅使用 Python 3.10+ 标准库（`argparse`、`subprocess`、`pathlib`、`shutil`、`enum`、`dataclasses`）
3. **失败要响**：所有子进程默认 `check=True`，失败立即以子进程的退出码退出
4. **可观察**：`-v` / `--verbose` 打印每条 subprocess 命令（含 cwd、env 改动）
5. **可静默化**：自动检测 TTY 与 `NO_COLOR` 环境变量，CI 环境下输出无 ANSI 代码

---

## 5. 子命令参考

所有子命令支持 `--help`，形如：`./misc/scripts/rover-cli <command> --help`。

全局选项（在子命令名 **之前**）：

| 选项 | 含义 |
|------|------|
| `-v` / `--verbose` | 打印 trace 日志（每条 subprocess 调用） |
| `-d` / `--debug` | 操作 Debug 构建（默认） |
| `-r` / `--release` | 操作 Release 构建 |
| `-V` / `--version` | 打印 CLI 版本 |

### 5.1 `rover configure`

为给定构建类型生成 Ninja 构建树。运行 `cmake -S . -B build/<type> -G Ninja -DCMAKE_BUILD_TYPE=<Type>`。**幂等**，可反复执行。

| 选项 | 含义 |
|------|------|
| `--reconfigure` | 删除 `CMakeCache.txt` 后再配置（强制重新检测） |
| `--no-editor` | `-DROVER_EDITOR=OFF` |
| `--no-tests` | `-DROVER_TESTS=OFF` |
| `--no-vulkan` | `-DROVER_VULKAN=OFF` |
| `--werror` | `-DROVER_WARNINGS_AS_ERRORS=ON` |
| `--asan` / `--ubsan` / `--tsan` | 启用 sanitizer（仅 Debug；TSan 与 ASan/UBSan 互斥） |
| `-D VAR=VAL` | 任意 CMake 定义（可重复） |

### 5.2 `rover build`

运行 `cmake --build build/<type>`。如果未配置，自动以默认参数调用 configure。

| 选项 | 含义 |
|------|------|
| `-t TARGET` / `--target TARGET` | 仅构建指定 target（如 `rover`, `rover_tests`, `rover_core`） |
| `-j N` / `--jobs N` | 并行任务数（默认 ninja 自动） |
| `--clean-first` | 先 clean 再 build（增量构建出错时使用） |

### 5.3 `rover clean`

删除构建产物。

| 选项 | 含义 |
|------|------|
| 默认（无标志） | 删除 `build/<type>/` |
| `--all` | 同时删除 `bin/<type>/` |
| `--everything` | 删除整个 `build/` + `bin/` + 顶层 `compile_commands.json`（忽略 `--all`、`-r/-d`） |

### 5.4 `rover run`

构建（除非 `--no-build`）后启动 `bin/<type>/rover`。`--` 之后的参数透传给引擎。

| 选项 | 含义 |
|------|------|
| `--validation` | 设置 `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` 与 `VK_LAYER_PATH`（来自 SDK） |
| `--no-build` | 跳过隐式 build 步骤（假设可执行文件已是最新） |
| `--executable NAME` | 替换为 `bin/<type>/<NAME>`（默认 `rover`） |
| `extra...` | `--` 后的参数透传给引擎 |

示例：
```bash
./misc/scripts/rover-cli run                        # 启动 Debug 构建
./misc/scripts/rover-cli -r run --validation        # Release + 验证
./misc/scripts/rover-cli run -- --some-arg foo      # 透传引擎参数
```

### 5.5 `rover debug`

在 gdb 下运行引擎。构建后 `gdb -ex run --args bin/<type>/rover`。

| 选项 | 含义 |
|------|------|
| `--validation` | 同 `run` |
| `--break SYMBOL` | 在符号处下断点（可重复，多次添加 `-ex break ...`） |
| `--batch` | 非交互模式：运行直到崩溃，自动打印 `bt full` 与所有线程 backtrace，然后退出 |
| `--no-build` | 跳过隐式 build |
| `--executable NAME` | 替换可执行文件名 |
| `extra...` | `--` 后参数透传给引擎 |

示例：
```bash
./misc/scripts/rover-cli debug --validation                      # 交互式
./misc/scripts/rover-cli debug --batch --validation              # 一次性捕捉崩溃栈
./misc/scripts/rover-cli debug --break 'rover::run_main_loop'    # 在主循环入口断点
```

### 5.6 `rover test`

构建并运行 `bin/<type>/rover_tests`（doctest）。

| 选项 | 含义 |
|------|------|
| `--filter PATTERN` | 仅运行匹配的测试用例（doctest `--test-case=`） |
| `--list` | 列出所有测试用例不运行 |
| `--no-build` | 跳过隐式 build |
| `extra...` | `--` 后参数透传给 doctest |

### 5.7 `rover format`

对所有第一方源码（`core/`、`drivers/`、`platform/`、`services/`、`modules/`、`editor/`、`main/`、`tests/`）运行 clang-format。**vendor/ 永不被遍历**。

| 选项 | 含义 |
|------|------|
| 默认 | 原地格式化（`clang-format -i`） |
| `--check` | 只检查不改写（`--dry-run --Werror`），有 drift 时退出非 0（CI 用） |
| `--paths PATH...` | 仅格式化指定路径（相对仓库根；可多个，可以是文件或目录） |

支持的扩展名：`.h`, `.hpp`, `.c`, `.cpp`, `.cc`, `.cxx`, `.inl`

### 5.8 `rover shaders`

强制重编 GLSL shader：触发 `main/shaders/*.glsl` 的 mtime（CMake/ninja 据此重新调用 `glslangValidator`），然后增量构建。

| 选项 | 含义 |
|------|------|
| `--no-build` | 仅 touch shader 源文件，不触发 build |

### 5.9 `rover status`

只读健康检查报告。打印：

- 项目路径与当前选定的构建类型
- 每个构建类型的配置状态（已配置 ✓ / 未配置 ·）
- 每个可执行文件的存在性与最近修改时间
- 工具链版本（cmake / ninja / clang-format / gdb / Vulkan SDK）

无副作用，可放心运行。

---

## 6. 核心模块

### 6.1 `rover.config`

```python
class BuildType(str, Enum):
    DEBUG, RELEASE
    @property
    def cmake_name(self) -> str:  # "Debug" / "Release"

@dataclass(frozen=True)
class ProjectPaths:
    root: Path
    
    @classmethod
    def discover(cls, start=None) -> ProjectPaths:
        """Walk up looking for CMakeLists.txt with `project(RoverEngine`."""
    
    # Path properties: build_dir, bin_dir, vendor_dir, docs_dir, misc_dir,
    #                  scripts_dir, shaders_dir
    # Per-build-type:  build_dir_for(t), bin_dir_for(t), executable(t, name),
    #                  is_configured(t), cmake_cache(t)
    # Source globs:    first_party_dirs() -> list[Path]
```

`discover()` 故意通过 `"project(RoverEngine"` 字符串识别根目录，避免与 `vendor/SDL/CMakeLists.txt` 等子目录混淆。

### 6.2 `rover.log`

| 函数 | 颜色 | 用途 |
|------|------|------|
| `trace(msg)` | 灰 | 仅 `--verbose` 时输出（subprocess 调用、SDK 发现细节） |
| `info(msg)` | 蓝 | 一般信息 |
| `step(msg)` | 粗体青 | 重要步骤（"Configuring..."、"Building..."） |
| `warn(msg)` | 黄 | 非致命警告 |
| `error(msg)` | 红 | 错误（不退出） |
| `success(msg)` | 绿 | 操作成功 |
| `fatal(msg, code=1)` | 粗体红 | 错误并以 `code` 退出 |

自动适配：
- stderr 不是 TTY → 关闭颜色
- 设置 `NO_COLOR` 环境变量 → 关闭颜色

### 6.3 `rover.shell`

```python
def run(argv, *, cwd=None, env=None, check=True, capture=False, inherit_env=True)
    -> subprocess.CompletedProcess
```

- `argv` 接受 `str` 或 `Path`
- `env` 默认与 `os.environ` 合并；`inherit_env=False` 用于纯净环境（测试用）
- `check=True`（默认）失败时以子进程退出码退出 CLI 进程
- `capture=True` 时捕获 stdout/stderr 不流式输出
- `--verbose` 时打印每条命令（含 cwd）

```python
def which(name) -> Path | None      # shutil.which 包装
def require(name, hint="") -> Path  # 必需的工具，找不到则 fatal
```

### 6.4 `rover.vulkan`

```python
def find_vulkan_sdk() -> Path | None
    # 解析顺序：VULKAN_SDK env var → glslangValidator 上两层目录 → None

def validation_env() -> dict[str, str]
    # 返回 VK_INSTANCE_LAYERS（必有）+ VK_LAYER_PATH（若 SDK 找到）
```

`run --validation` / `debug --validation` 调用 `validation_env()` 把环境变量合并到子进程。

---

## 7. 扩展指南：添加新子命令

新增子命令 `<name>` 步骤：

1. **创建 `misc/scripts/rover/commands/<name>.py`**：

   ```python
   """``rover <name>`` -- one-line description."""

   from __future__ import annotations
   import argparse
   from rover import log, shell
   from rover.config import BuildType, ProjectPaths

   NAME = "<name>"
   HELP = "Short help shown by ``rover --help``."
   DESCRIPTION = "Optional longer description shown by ``rover <name> --help``."

   def add_args(parser: argparse.ArgumentParser) -> None:
       parser.add_argument(...)

   def run(paths: ProjectPaths, build_type: BuildType, args: argparse.Namespace) -> int:
       log.step("Doing the thing")
       # ... use shell.run, paths.*, etc.
       return 0  # exit code
   ```

2. **在 `commands/__init__.py` 的 `ALL_COMMANDS` 列表中添加引用**：

   ```python
   from rover.commands import (..., my_new_command, ...)
   ALL_COMMANDS = [..., my_new_command, ...]
   ```

3. **更新本文档（`docs/dev/MISC.md`）的 [子命令参考](#5-子命令参考)** 一节

4. **测试**：`./misc/scripts/rover-cli <name> --help`

### 接口契约

每个子命令模块 **必须** 提供：

| 标识 | 类型 | 说明 |
|------|------|------|
| `NAME` | `str` | 出现在 argparse 子命令列表的名称 |
| `HELP` | `str` | 一行简介 |
| `DESCRIPTION` | `str` 或省略 | argparse 详情；省略时回落到 `HELP` |
| `add_args(parser)` | function | 在传入的 ArgumentParser 上注册参数 |
| `run(paths, build_type, args)` | function → int | 执行命令；返回退出码 |

**禁止**：
- 在模块顶层做 I/O 副作用
- 修改 `os.environ`（用 `shell.run` 的 `env` 参数）
- 调用 `sys.exit()`（用返回值或 `log.fatal`）
- 添加 pip 依赖

---

## 8. 设计原则

### 8.1 单一入口 vs. 多个零散脚本

我们 **故意** 选择了一个 `rover-cli` 多子命令架构而非 `build.sh` / `clean.sh` / `run.sh` 等独立脚本，原因：

| 维度 | 单入口 | 多脚本 |
|------|--------|--------|
| 学习成本 | 一次性掌握 `--help` | 每个脚本独立学习 |
| 共享基础设施 | 自然复用 | 容易复制粘贴 / 漂移 |
| 子命令组合 | 容易（`build` 内部调用 `configure`） | 脆弱（脚本互调） |
| 跨平台 | Python 抹平差异 | bash / cmd 各写一遍 |
| 自动补全 | `argparse` + `argcomplete` 现成 | 手写 |
| 维护 | 集中 | 分散，易腐烂 |

### 8.2 Python 而非 bash

- **跨平台**：Linux / macOS / Windows 通吃
- **可测试**：可以 `python -m unittest` 覆盖各模块
- **结构化数据**：dataclass、enum、Path 比 bash 字符串拼接安全
- **错误处理**：异常机制比 `set -euo pipefail` 更精细

### 8.3 零依赖

仅使用 Python 3.10+ 标准库。理由：
- 引擎开发者不应被迫 `pip install` 一堆东西
- 没有 venv / 虚拟环境管理负担
- 升级 Python 时不会因依赖锁版本而卡住

3.10+ 是因为我们使用：
- 联合类型语法 `X | Y`（PEP 604）
- 结构化模式匹配（potential future use）
- `dataclasses`（自 3.7）

### 8.4 失败要响

- 所有 subprocess 默认 `check=True`
- 找不到必需工具时直接 `fatal()` 并给出安装提示（`hint`）
- 退出码透传：CLI 退出码等于失败的子命令退出码（CI 可用）

### 8.5 颜色降级

- 自动检测 TTY（`sys.stderr.isatty()`）
- 尊重 `NO_COLOR=1`
- CI 中输出干净的纯文本日志

### 8.6 路径独立

- `ProjectPaths.discover()` 从任何 cwd 都能找到仓库根
- 用户可以 `cd vendor/SDL && ../misc/scripts/rover-cli build` 而仍然构建 Rover

---

## 9. CMake 模块（misc/cmake/）

| 文件 | 内容 |
|------|------|
| `RoverOptions.cmake` | `option(ROVER_*)` 编译开关 + `ROVER_PLATFORM` 自动检测 |
| `RoverCompiler.cmake` | `Rover::CompileFlags` interface library（warnings、sanitizers、平台宏） |
| `RoverUtils.cmake` | `rover_add_library`、`rover_glob_sources`、`rover_collect_modules`、`rover_generate_module_registration` |
| `RoverShader.cmake` | `rover_add_shader`（GLSL → SPIR-V → C 头文件） |
| `RoverVersion.h.in` | `configure_file` 模板，生成 `rover_version.h` |

### `rover_add_shader(<output_var> SHADER ... STAGE ... VAR ... OUTPUT_DIR ...)`

将 GLSL 源文件编译为 `static const uint32_t <VAR>[] = { ... };` 形式的 C 头文件，使用 `glslangValidator -V --vn`。

- 自动通过 `find_program` + `VULKAN_SDK` env 定位 `glslangValidator`
- 目标 Vulkan 1.3
- 生成的头文件路径追加到 `<output_var>` 列表，可作为 target SOURCES 加入构建依赖

详见 [DRIVERS.md §15.2](DRIVERS.md#152-着色器编译)。

### 修改 CMake 模块时的注意事项

- 每个 `.cmake` 文件以 `include_guard(GLOBAL)` 开头，防止重复包含
- 函数命名使用 `rover_` 前缀，避免污染全局命名空间
- 不要在模块顶层做 `find_package`（除非真的需要全局可用），优先在调用 target 的 CMakeLists.txt 中按需 `find_package`
- 修改后必须运行 `./misc/scripts/rover-cli configure --reconfigure` 验证

---

## 10. 后续工作

### 已完成

- [x] CLI 包结构（`misc/scripts/rover/`）
- [x] 核心模块：config / log / shell / vulkan
- [x] 子命令：configure / build / clean / run / debug / test / format / shaders / status
- [x] CMake 模块：Options / Compiler / Utils / Shader / Version
- [x] 自动 Vulkan SDK 发现（`VULKAN_SDK` env、`glslangValidator` 二进制）
- [x] TTY/`NO_COLOR` 自适应彩色日志

### 待做

- [ ] **`watch` 子命令**：监视源文件变化，自动 build（基于 `inotify` / `fsevents`，可选依赖）
- [ ] **`bench` 子命令**：运行 benchmark 套件（待 benchmark 框架到位）
- [ ] **`lint` 子命令**：clang-tidy 集成（项目根 `.clang-tidy` 已就绪）
- [ ] **`pkg` 子命令**：打包发布（zip / installer）
- [ ] **`doc` 子命令**：生成 / 验证 API 文档
- [ ] **shell 自动补全**：`argcomplete` 或手动 bash/zsh 补全脚本
- [ ] **CI 集成示例**：GitHub Actions workflow 调用 `rover-cli`
- [ ] **单元测试**：`tests/scripts/` 下覆盖 ProjectPaths.discover、shell.run 等
- [ ] **Windows / macOS 验证**：脚本本身跨平台，但需要测试 CMake/Ninja/gdb 路径

---

*Rover Engine Misc Tooling v0.1.0 — 文档版本与脚本同步。*
