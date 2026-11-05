# Phase 2 场景加载

mesh demo 从 JSON `.rscene` 文件加载实际渲染的 World。文件不存在时生成默认相机、平行光、立方体并保存；文件已存在时直接加载，不覆盖用户修改。

## 运行

在仓库根目录，使用已有 CMake 构建目录：

```bash
cmake --build build -j 4
./bin/debug/rover --scene /tmp/rover-demo.rscene
```

首次运行后关闭窗口，修改 `/tmp/rover-demo.rscene` 中网格实体的 `transform.position.x`，然后再次执行相同命令。物体位置应随文件改变。相机位置、投影参数、灯光颜色、方向、强度与父子变换也由加载后的组件决定。viewport 的宽高比在运行时按窗口更新。

有限帧验证：

```bash
./bin/debug/rover --scene /tmp/rover-demo.rscene --frames 5
```

未传 `--scene` 时使用当前工作目录的 `demo.rscene`。`--demo triangle` 保留原始三角形演示；`--frames` 只用于 mesh demo。场景所在父目录须已存在。无效场景会报错退出，保留原文件。

## 场景格式与网格资源

场景根包含 `version: 2` 与 `entities` 数组。每个实体的 `components.mesh` 仅持久化一个字符串：

```json
{
  "mesh_asset": "builtin://meshes/cube"
}
```

当前支持的内置网格：

| 路径 | 几何体 |
|------|--------|
| `builtin://meshes/cube` | 边长 1、中心在原点的立方体 |
| `builtin://meshes/quad` | XY 平面上的单位 quad |
| `builtin://meshes/sphere` | 半径 1 的 UV 球 |

修改 `transform.scale` 调整尺寸。外部网格路径暂不支持，未知或空路径会明确报错。材质目前使用 demo 的固定无纹理着色；纹理上传工具仍存在，但纹理/采样器句柄不会写入场景。

`MeshAssetCache` 在 serialization 模块内根据路径生成 CPU 网格，通过抽象 `GraphicsDevice` 上传；同路径共享缓冲。缓存拥有 GPU 缓冲，组件只借用。缓存释放前等待 GPU 完成，且必须早于设备关闭销毁。重新 `clear()` 后，旧组件中的句柄不得用于绘制，需再次 `resolve()`。

## 序列化行为

- JSON 将向量表示为 `{x,y,z}`、四元数表示为 `{w,x,y,z}`，加载器恢复数学类型；RBIN 保留对应 Variant 类型。
- 浮点数输出保留足以往返的精度。
- 成功加载会替换目标 World；无效版本、组件结构、变换值或父子链不会改变原 World。
- 父子关系以 `entities` 数组的索引保存，加载时重建父引用与全部子节点列表；循环链与无效索引被拒绝。
- GPU 缓冲、纹理、采样器句柄和顶点计数均为运行时数据，不保存。
- version 1 的组件数据仍可加载，旧 GPU 句柄会被忽略；网格须补充 `mesh_asset` 才能恢复。JSON demo 仅加载 JSON，RBIN 可通过 `SceneSerializer::load_binary` 使用。

## 验证

`tests/modules/scene_loading_test.cpp` 覆盖 JSON/RBIN 真实文件往返、相机/变换/灯光恢复、多子节点层级、稳定资源引用、旧句柄丢弃、无效场景保留原 World、缓冲共享、上传失败清理与重试。测试使用 RAII 临时目录与 GraphicsDevice mock，无需 GPU。

```bash
ctest --test-dir build --output-on-failure
```

相关决策：[ADR-0011](../product/adr/ADR-0011-scene-mesh-resource-paths.md)。Phase 2 的依赖 DAG 与跨 pass 自动 barrier 仍在后续范围内。

2026-11-05 本地验证：Debug 构建成功；全量 128 个测试、777 条断言通过；任务队列与 JobSystem 测试连续运行 25 次通过。在 Intel Iris Xe / Vulkan 1.3 上启用现有验证层，首次创建与修改后重载各完成 5 帧；未知资源与损坏 JSON 均按预期退出并保留文件，未记录验证层 error。驱动初始化仍有既有的 `VK_KHR_get_physical_device_properties2` 被列为 device extension 的 warning，另行处理。

本次修改的 C++ 文件通过格式检查；全库格式检查仍报告未修改的 `core/graphics/graphics_device.h`、`core/variant/callable.h`、`drivers/vulkan/vk_instance.cpp`、`tests/core/event_test.cpp` 存在格式差异。
