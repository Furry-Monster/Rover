# Rover Phase 2 示例场景

本目录存放六个 Phase 2 示例场景，包含默认立方体。引擎资源的目录约定见 [builtin_assets/README.md](../../README.md)。

在仓库根目录构建后，通过 `--scene` 加载：

```bash
cmake --build build -j 4
./bin/debug/rover --scene main/builtin_assets/scenes/demos/primitives.rscene
```

| 场景 | 内容 | 运行参数 |
|------|------|----------|
| [默认立方体](cube.rscene) | 立方体、透视相机与平行光 | 默认加载，或 `--scene main/builtin_assets/scenes/demos/cube.rscene` |
| [几何体展台](primitives.rscene) | 立方体、球体、倾斜 quad 与展台，冷色平行光 | `--scene main/builtin_assets/scenes/demos/primitives.rscene` |
| [父子层级](hierarchy.rscene) | 旋转总装、四条支臂、二级卫星与缩小的独立组件 | `--scene main/builtin_assets/scenes/demos/hierarchy.rscene` |
| [深度遮挡](depth.rscene) | 前后交错的立方体、球体、薄板与背景，验证深度遮挡 | `--scene main/builtin_assets/scenes/demos/depth.rscene` |
| [正交建筑](orthographic.rscene) | 正交相机下的六座高低建筑、屋顶与连桥 | `--scene main/builtin_assets/scenes/demos/orthographic.rscene` |
| [棋盘庭院](courtyard.rscene) | 25 块高低交错地砖、六根立柱与中心雕塑，共 40 个网格 | `--scene main/builtin_assets/scenes/demos/courtyard.rscene` |

例如：

```bash
./bin/debug/rover --scene main/builtin_assets/scenes/demos/hierarchy.rscene
./bin/debug/rover --scene main/builtin_assets/scenes/demos/depth.rscene
./bin/debug/rover --scene main/builtin_assets/scenes/demos/orthographic.rscene
./bin/debug/rover --scene main/builtin_assets/scenes/demos/courtyard.rscene
```

这些是静态场景，使用现有的无纹理着色和单平行光；无需下载外部资产。关闭窗口退出，附加 `--frames 5` 可有限帧运行。不传 `--scene` 时加载本目录的 `cube.rscene`；`--demo triangle` 使用三角形演示。

场景加载不会改写文件。可复制一份修改后重新运行：

```bash
cp main/builtin_assets/scenes/demos/hierarchy.rscene /tmp/my-hierarchy.rscene
./bin/debug/rover --scene /tmp/my-hierarchy.rscene
```

调整 `components.transform` 中的 `position`、`rotation`、`scale`，或更换 `components.mesh.mesh_asset`。hierarchy 场景中的 `parent.index` 指向 `entities` 数组位置，修改父节点会影响整组子节点。正交场景中，`camera.ortho_size` 表示可视范围的半高。

加载格式与资源生命周期见 [SCENE_LOADING.md](../../../../docs/dev/SCENE_LOADING.md)。

本地验证：5 个场景均通过真实 Vulkan 加载并完成 5 帧渲染，未记录验证层 error，场景文件未改写；全量 130 个测试、972 条断言通过。新增回归检查覆盖球体外向绕序与正交投影的 Y 方向。
