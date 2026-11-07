# ADR-0008: vendor 第三方依赖统一手动下载，禁用 submodule / FetchContent / find_package

- **Status**: Accepted
- **Date**: 2026-04-29
- **Deciders**: core team

## Context

决策时，[依赖规范](../../standard/DEPENDENCIES.md) v0.1 推荐 git submodule，而仓库中的 SDL、ImGui、spdlog 等采用普通源码目录，Vulkan 头文件通过 `find_package(Vulkan REQUIRED)` 获取。为统一后续依赖集成，比较以下方式：

- **git submodule**：
  - 克隆后需要递归拉取子模块，CI 和新环境需额外执行该步骤。
  - submodule 指针漂移、detached HEAD、与上游 fork 协作复杂。
  - 在中国大陆等网络受限环境下 `git clone https://github.com/...` 速度极不稳定，submodule update 经常超时。
- **CMake `FetchContent` / `ExternalProject`**：
  - configure 阶段联网下载，受上游服务可用性和镜像状态影响。
  - 首次配置需要联网获取依赖。
  - 缓存放在 `_deps/`，体积巨大且不进入 git 历史，难以审计。
- **`find_package` 系统库**：
  - 每位开发者本机环境差异大（Vulkan SDK 版本、Linux 发行版自带的 SDL 版本…），同一份代码在 A 机器编过、B 机器编不过。
  - 与「单条命令克隆即构建」的 onboarding 体验冲突。
  - CI 镜像维护成本高（要预装一长串系统库）。

依赖管理目标是源码自包含、版本可复现和离线构建。

## Decision

**vendor 第三方依赖一律以「源码副本」形式手动下载到 `vendor/<libname>/` 并提交进 Rover 仓库。禁止以下三种集成方式：**

| 方式 | 状态 | 原因 |
| -- | -- | -- |
| `git submodule` | 禁止 | 需要额外拉取步骤，与离线构建目标冲突 |
| CMake `FetchContent` / `ExternalProject` | 禁止 | configure 阶段联网，源码获取依赖外部服务 |
| `find_package` 探测系统库 | 禁止 | 依赖开发者本机环境，破坏可复现性 |
| 手动下载源码副本到 `vendor/<libname>/` | 采用 | 源码随仓库提供，可审计、可离线 |

具体规则：

1. **源码副本 = 仓库内容**：`vendor/<libname>/` 下放上游 release tarball / 指定 tag 的纯源码（**不**保留 `.git/`）。这部分代码进入 Rover 主仓库的 git 历史。
2. **下载工具**：提供 `misc/scripts/vendor/` 下的脚本作为标准入口（如 `misc/scripts/vendor/fetch.py <lib>`），脚本职责：
   - 从 `misc/scripts/vendor/manifest.toml`（或等价清单）查 URL / 版本 / SHA-256
   - 下载 tarball / zip
   - 校验 SHA-256
   - 解压到 `vendor/<libname>/`
   - 删除多余文件（`.git/`、tests/、docs/、examples/，按 manifest 规则）
   - 输出新增 / 变更摘要供贡献者 `git add` 审查
3. **手动下载兜底**：若脚本不可用（如新库尚未编入 manifest），允许人工 `wget`/`curl`/`tar -xzf`，但 PR 中**必须**：
   - 在 `manifest.toml` 中补登记该库
   - 在 ADR / commit message 中说明来源 URL 与版本号
4. **vendor/CMakeLists.txt 包装**：所有库通过 `add_subdirectory(...)` 或手写 INTERFACE / STATIC target 暴露成 `Rover::<Lib>` 别名（保持现状）。
5. **升级 = 重新跑 fetch 脚本**：升级第三方库 = 改 manifest 中的版本号 → 跑脚本 → 测试 → commit；diff 直接显示在 PR 里，便于 review。
6. **Vulkan 迁移**：将 `Vulkan-Headers` 源码纳入 `vendor/Vulkan-Headers/`，替换决策时的 `find_package(Vulkan REQUIRED)`。构建使用仓库中的头文件，运行期由 volk 通过 `dlopen("libvulkan.so.1")` 加载系统驱动入口。

## Consequences

**Positive:**

- **依赖随仓库提供**：完成头文件迁移后，库源码与头文件通过普通 `git clone` 获取，构建期间可离线使用。
- **构建确定性**：vendor 内容被 git 锁死，跨机器、跨时间得到比特一致的输入。
- **离线 / 内网友好**：CI 镜像 / 离线开发机 / 安全沙箱环境下都可构建。
- **审计透明**：vendor 升级 = git diff 可见，安全审查与 license 合规更容易。
- **集成调整可追踪**：CMake 配置与 wrapper 的变更进入 git 历史，源码维护规则见 [依赖规范 §1.3](../../standard/DEPENDENCIES.md#13-vendor-源码维护)。

**Negative / Trade-offs:**

- **仓库体积**：源码副本预计增加 50–200 MB，实际大小随依赖数量变化；超过 500 MB 时通过新 ADR 调整策略。
- **升级流程**：每次升级提交源码差异并运行验证，规划中的 fetch 脚本负责自动下载和校验。
- **必须维护 manifest**：每库一行，长期维护成本低（约等于一份 `requirements.txt`）。
- **首次迁移**：清理 vendor 目录中的 `.git/` 与上游冗余文件。
- **Vulkan 头文件**：替换 `find_package(Vulkan REQUIRED)` 的工作列入 Phase 2。

## Alternatives Considered

- **维持 git submodule 主线**：额外拉取步骤依赖网络，与离线开发目标冲突。放弃。
- **CMake `FetchContent`**：configure 阶段联网破坏可复现性；缓存与 git 历史分离，难审计。放弃。
- **混合方案**（小库 vendor、大库 submodule）：规则二元化制造决策成本，且谁是「大库」边界模糊。放弃。
- **包管理器**（vcpkg / Conan / xrepo）：引入第二套依赖系统，与 ADR-0002「单一 CMake 构建」精神冲突；vcpkg 的 baseline 漂移与 Conan 的 profile 复杂度都不在我们想接受的范围内。放弃。

## References

- [`docs/standard/DEPENDENCIES.md`](../../standard/DEPENDENCIES.md) §1.2 添加新依赖的流程
- [`vendor/CMakeLists.txt`](../../../vendor/CMakeLists.txt)
- [ADR-0002 单一 CMake 构建系统](ADR-0002-cmake-as-single-build-system.md)
- 计划：`misc/scripts/vendor/fetch.py` 与 `misc/scripts/vendor/manifest.toml`（待 Phase 2 落地）
