# Phase 2 场景加载

mesh demo 从 JSON `.rscene` 文件加载并渲染 World。文件不存在时生成默认相机、平行光、立方体并保存；文件已存在时直接加载，保留用户修改。

默认立方体与几何体展台、父子层级、深度遮挡、正交建筑、棋盘庭院共六个场景统一存放在 `main/builtin_assets/scenes/demos/`，运行方式见 [示例说明](../../main/builtin_assets/scenes/demos/README.md)。

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

未传 `--scene` 时加载 `main/builtin_assets/scenes/demos/cube.rscene`，在仓库根目录运行。`--demo triangle` 保留原始三角形演示；`--frames` 用于所有 demo。`--demo frame-graph` 验证离屏渲染与 GPU 像素回读，默认运行 8 帧，见 [Frame Graph](FRAME_GRAPH.md)。场景所在父目录须已存在。无效场景会报错退出，保留原文件。

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

修改 `transform.scale` 调整尺寸。加载器接受上表中的内置路径，外部、未知或空网格路径会报错。demo 使用固定无纹理场景材质，渲染结果经过 HDR 离屏、曝光/Reinhard 映射与交换链呈现；参数、回读和缩放验证见 [离屏后处理](POST_PROCESS.md)。纹理上传由工具接口提供，纹理和采样器句柄作为运行时数据管理。

`MeshAssetCache` 在 serialization 模块内按路径生成 CPU 网格，通过抽象 `GraphicsDevice` 上传；同路径共享缓冲。缓存拥有 GPU 缓冲，组件借用句柄。销毁缓存前等待 GPU 完成，缓存销毁后再关闭设备。调用 `clear()` 后，重新 `resolve()` 再使用组件绘制，旧句柄失效。

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

相关决策：[ADR-0011](../product/adr/ADR-0011-scene-mesh-resource-paths.md)。Frame Graph 的依赖 DAG 与单队列跨 pass 自动 barrier 已实现，见 [使用契约](FRAME_GRAPH.md)。

历史结果见 [2026-11-05 场景加载验证记录](../debug/SCENE_LOADING_VALIDATION.md)。
