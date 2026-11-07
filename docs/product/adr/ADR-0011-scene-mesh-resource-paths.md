# ADR-0011: Phase 2 场景使用稳定网格路径

- **Status**: Accepted
- **Date**: 2026-11-05
- **Deciders**: core team

## Context

场景当前保存进程内 GPU 句柄，重启后无法恢复；mesh demo 也只检查加载结果的实体数，实际仍渲染原始 World。JSON 中的向量与四元数字典需要显式恢复为数学类型。

## Decision

场景格式升级到 version 2。`MeshComponent::mesh_asset` 保存稳定路径，GPU 缓冲、纹理、采样器句柄与顶点计数只作为运行时数据，均不写入场景。scene 模块只持有字符串，不依赖 serialization 模块。

`modules/serialization/MeshAssetCache` 通过抽象 `GraphicsDevice` 加载并共享内置网格：`builtin://meshes/cube`、`builtin://meshes/quad`、`builtin://meshes/sphere`。缓存拥有缓冲区，组件借用句柄；缓存必须早于设备关闭销毁。未识别或缺失的路径报错，不用默认立方体掩盖资源缺失。

加载成功后替换目标 World，失败保留原 World。支持 version 1 的组件数据，但忽略其旧 GPU 句柄；旧场景的网格需要补充 `mesh_asset` 后才能恢复。JSON 与二进制使用相同的场景语义，JSON 加载器在组件边界恢复向量和四元数。

mesh demo 使用 `--scene <path>` 指定持久场景，默认加载 `main/builtin_assets/scenes/demos/cube.rscene`。仅在文件不存在时生成默认场景，然后从文件加载实际渲染的 World。`--frames <positive-count>` 用于有限帧运行验证。

## Consequences

- 支持修改场景后重启恢复相机、灯光、变换和网格；同路径网格共享 GPU 缓冲。
- 内置路径无需进程内 AssetRegistry 编号，也不要求提前实现 Phase 3 的磁盘资产数据库。
- 本阶段仍使用无纹理基础材质；外部模型、材质、纹理的持久引用与导入由 Phase 3 扩展。
- 格式升级不尝试猜测旧 GPU 句柄对应的资产。

## References

- [ADR-0006](ADR-0006-io-split-platform-and-serialization.md)
- [架构文档](../../dev/ARCHITECTURE.md)
- [需求 F-406 / F-407](../REQUIREMENTS.md)
