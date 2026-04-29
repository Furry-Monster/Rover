# ADR-0008: vendor 第三方依赖统一手动下载，禁用 submodule / FetchContent / find_package

- **Status**: Accepted
- **Date**: 2026-04-29
- **Deciders**: core team

## Context

Rover 当前 vendor 引入方式在 [`docs/standard/DEPENDENCIES.md`](../../standard/DEPENDENCIES.md) v0.1 版本里写的是「首选 git submodule、次选 vendored 副本、不选 find_package」。但实际仓库状态（vendor/SDL/、vendor/imgui/、vendor/spdlog/ 等都是普通目录、没有 `.gitmodules`、Vulkan 走 `find_package(Vulkan REQUIRED)`）和文档表述存在偏差。同时随着引擎复杂度上升（Slang、KTX-Software、Basis Universal、Jolt 等候选库陆续要进来），三种集成方式各自的痛点逐渐放大：

- **git submodule**：
  - `git clone` 不带 `--recursive` 会得到一个无法构建的工作树；CI / 新贡献者频繁踩坑。
  - submodule 指针漂移、detached HEAD、与上游 fork 协作复杂。
  - 在中国大陆等网络受限环境下 `git clone https://github.com/...` 速度极不稳定，submodule update 经常超时。
- **CMake `FetchContent` / `ExternalProject`**：
  - configure 阶段需要联网下载，构建非确定性（上游 GitHub 抽风、镜像不一致都会导致结果变化）。
  - 离线构建不可能。
  - 缓存放在 `_deps/`，体积巨大且不进入 git 历史，难以审计。
- **`find_package` 系统库**：
  - 每位开发者本机环境差异大（Vulkan SDK 版本、Linux 发行版自带的 SDL 版本…），同一份代码在 A 机器编过、B 机器编不过。
  - 与「单条命令克隆即构建」的 onboarding 体验冲突。
  - CI 镜像维护成本高（要预装一长串系统库）。

我们想要的是 **hermetic（自包含）+ deterministic（可复现）+ offline-first** 的依赖体系：克隆仓库 → 不联网 → 构建即可成功。

## Decision

**vendor 第三方依赖一律以「源码副本」形式手动下载到 `vendor/<libname>/` 并提交进 Rover 仓库。禁止以下三种集成方式：**

| 方式 | 状态 | 原因 |
| -- | -- | -- |
| `git submodule` | ❌ 禁止 | 体验差、网络脆弱、与离线构建目标冲突 |
| CMake `FetchContent` / `ExternalProject` | ❌ 禁止 | configure 阶段联网，构建非确定 |
| `find_package` 探测系统库 | ❌ 禁止 | 依赖开发者本机环境，破坏可复现性 |
| 手动下载源码副本到 `vendor/<libname>/` | ✅ **唯一合法方式** | hermetic、可审计、可离线 |

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
6. **Vulkan 处理（澄清，非例外）**：当前 `find_package(Vulkan REQUIRED)` 不符合本 ADR，列入迁移 TODO。迁移目标：`Vulkan-Headers` 源码 vendor 进 `vendor/Vulkan-Headers/`；构建期完全不依赖系统 Vulkan SDK；运行期由 volk 通过 `dlopen("libvulkan.so.1")` 加载系统 ICD。运行期 ICD 加载是图形 API 的固有机制，不算「构建期探测系统库」，与本 ADR 不冲突。

## Consequences

**Positive:**

- **克隆即构建**：新贡献者 `git clone` 后立即 `cmake -B build && cmake --build build` 即可，不需要 `--recursive`、不需要预装系统 SDK、不需要联网。
- **构建确定性**：vendor 内容被 git 锁死，跨机器、跨时间得到比特一致的输入。
- **离线 / 内网友好**：CI 镜像 / 离线开发机 / 安全沙箱环境下都可构建。
- **审计透明**：vendor 升级 = git diff 可见，安全审查与 license 合规更容易。
- **patching 简单**：本地修改 vendor 直接是 git 修改，配合 ADR-0008 §1.3 的 wrapper / patch 政策即可。

**Negative / Trade-offs:**

- **仓库体积膨胀**：vendor/ 下放源码意味着 Rover 仓库整体变大（粗估 +50–200 MB，视引入库数量而定）。需要 git-lfs 兜底机制？目前评估为「可接受」（vendor 总量增长缓慢）；若超过 500 MB 再 ADR 调整。
- **升级显得"重"**：以前 `git submodule update` 一行命令；现在要跑脚本 + commit。但通过 `misc/scripts/vendor/fetch.py --upgrade <lib>` 自动化后差距很小。
- **必须维护 manifest**：每库一行，长期维护成本低（约等于一份 `requirements.txt`）。
- **首次迁移有 GLEs**：当前 vendor/ 多个目录还伴随 `.git/` / 上游冗余文件，需要 ADR 落地后逐步清理。
- **现有 `find_package(Vulkan REQUIRED)` 必须迁移**：列入 Phase 2 任务，不阻塞当前开发。

## Alternatives Considered

- **维持 git submodule 主线**：网络脆弱、新人 onboarding 容易翻车；与离线 / 内网开发目标冲突。放弃。
- **CMake `FetchContent`**：configure 阶段联网破坏可复现性；缓存与 git 历史分离，难审计。放弃。
- **混合方案**（小库 vendor、大库 submodule）：规则二元化制造决策成本，且谁是「大库」边界模糊。放弃。
- **包管理器**（vcpkg / Conan / xrepo）：引入第二套依赖系统，与 ADR-0002「单一 CMake 构建」精神冲突；vcpkg 的 baseline 漂移与 Conan 的 profile 复杂度都不在我们想接受的范围内。放弃。

## References

- [`docs/standard/DEPENDENCIES.md`](../../standard/DEPENDENCIES.md) §1.2 添加新依赖的流程
- [`vendor/CMakeLists.txt`](../../../vendor/CMakeLists.txt)
- [ADR-0002 单一 CMake 构建系统](ADR-0002-cmake-as-single-build-system.md)
- 计划：`misc/scripts/vendor/fetch.py` 与 `misc/scripts/vendor/manifest.toml`（待 Phase 2 落地）
