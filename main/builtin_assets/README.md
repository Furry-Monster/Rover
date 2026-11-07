# Rover 引擎内置资源

`main/builtin_assets/` 统一存放引擎附带的资源，按资源类型划分目录。

当前目录：

```text
main/builtin_assets/
├── README.md
├── shaders/                 Slang 源与编译约定
└── scenes/
    └── demos/
        ├── README.md
        ├── cube.rscene
        ├── primitives.rscene
        ├── hierarchy.rscene
        ├── depth.rscene
        ├── orthographic.rscene
        └── courtyard.rscene
```

示例场景集中在 `scenes/demos/`，其他内置场景放在 `scenes/` 的对应用途目录。后续模型、纹理、材质、字体、音频分别使用 `models/`、`textures/`、`materials/`、`fonts/`、`audio/`，随实际资源加入时创建，避免与场景混放。资源来源、许可证与使用方式应记录在对应目录的说明中。

在仓库根目录运行：

```bash
./bin/debug/rover --scene main/builtin_assets/scenes/demos/primitives.rscene
```

默认启动加载 `scenes/demos/cube.rscene`。六个示例的内容及完整运行命令见 [示例场景说明](scenes/demos/README.md)。内置 shader 见 [shader 说明](shaders/README.md)，编译流程见 [资源规范](../../docs/standard/ASSETS.md)。
