# ADR-0006: IO 拆分到 platform/ 与 modules/serialization/

- **Status**: Accepted
- **Date**: 2026-04-26
- **Deciders**: core team

## Context

Godot 把所有 IO 相关代码（文件、流、目录、压缩、各种格式的 import/export）放在 `core/io/`。这导致：

- 平台相关的文件 API（`fopen` vs `CreateFile` vs Android `AAsset`）和业务相关的资源序列化（资产 import 工作流）混在一起
- 想替换序列化格式（如换用 FlatBuffers / cbor / 自定义二进制），要触动 core
- 想为 Web/Android 这种文件系统语义不同的平台特化文件 API，要同时改多个文件

两类问题的解决节奏与所有者根本不同：底层文件 API 跟随平台 SDK 演进；序列化格式跟随业务需求演进。

## Decision

把 IO 拆为 **两层**：

1. **底层文件 / VFS** —— 平台相关
   - 抽象接口在 `core/os/`（未来）：`FileSystem`、`File`、`DirIterator` 等
   - 具体实现在 `platform/<os>/file.cpp`
   - 跟 Window / EventPump 一样由 `platform/<os>/` façade 注入
2. **资源序列化 / Asset 格式** —— 业务相关
   - 实现在 `modules/serialization/`
   - 可被启用 / 禁用 / 替换的模块
   - 包含格式定义、import 工作流、export 工作流

注意：当前 Phase 1 只用到了 shader（编译期内联为 SPIR-V 头文件），尚未实现这两层中的任何一个。Phase 2 会先实现 platform 层文件 API，再写 serialization 模块。

## Consequences

**Positive:**

- 平台移植只需替换 `platform/<os>/file.cpp`，不影响序列化逻辑
- 替换序列化格式只是换一个 module，不影响平台
- 测试容易：可以为序列化模块写单元测试（基于 `std::filesystem` 即可，不需要平台层）
- 与 ADR-0001 的依赖倒置一致

**Negative / Trade-offs:**

- 比 Godot 风格多一个间接层（FileSystem 抽象 → 平台实现 → 序列化使用）
- 短期内 Phase 1 没有 IO 需求，这个 ADR 只是定方向，没有立刻产生代码
- 跨平台 VFS（如打包成 .pak 后的虚拟路径）需要在抽象层考虑，初版可能简化为「只支持原生路径」

## Alternatives Considered

- **保留 Godot 风格 `core/io/`**：合并两类问题，不利于演进。放弃。
- **把 IO 全部放到 `services/io/`**：service 是单例语义，但文件读写不必须单例（多个独立 FileSystem 实例也合理，比如不同的 mount point）。放弃。
- **单一 `modules/io/` 同时管文件 API 与序列化**：模块依赖平台层，无法把平台 API 抽象到核心。放弃。

## References

- [`docs/dev/ARCHITECTURE.md`](../../dev/ARCHITECTURE.md) §3 目录结构
- [`docs/dev/PLATFORM.md`](../../dev/PLATFORM.md) §12 后续工作（含「文件系统抽象」TODO）
- 未来：`docs/standard/ASSETS.md` 详述 asset 工作流约束
