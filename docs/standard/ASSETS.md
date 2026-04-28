# 资源处理工作流

> 本规范定义 Rover 引擎对 **资源（Assets）** 的格式约束、目录组织、import / export 工作流。当前 Phase 1 仅有 shader，本文档同时规划 Phase 2+ 的格式选型与实现路径。

---

## 1. 资源类型与格式

| 资源类别 | 源格式（人类编辑） | 中间格式（构建产物） | 运行时格式 | 当前状态 |
|---------|------------------|-------------------|-----------|---------|
| **Shader** | GLSL `.vert.glsl` / `.frag.glsl` | SPIR-V `.spv` | C 头文件（编译时内联）/ `.spv` 文件（运行时加载） | ✅ Phase 1 |
| **Texture** | `.png` / `.jpg` / `.tga` / `.exr` | KTX2（推荐）/ DDS / 自定义 `.rtex` | KTX2 → 解压到 GPU | 🎯 Phase 2 |
| **Model / Mesh** | `.gltf` / `.glb` / `.fbx`（评估） | 自定义 `.rmesh`（二进制） | `.rmesh` → 解析到 GPU | 🎯 Phase 2 |
| **Audio** | `.wav` / `.ogg` / `.mp3` | 直接使用（短音）或 streaming（长音） | 解码到 PCM 后混音 | 🎯 Phase 4 |
| **Font** | `.ttf` / `.otf` | MSDF atlas（推荐）/ bitmap atlas | 纹理 + glyph 元数据 | 🎯 Phase 3 |
| **Scene** | `.rscene`（自定义文本，json 或 KDL 评估中） | 同源 | 同源 | 🎯 Phase 2 |
| **Material** | `.rmat`（自定义文本） | 同源 | 同源 | 🎯 Phase 2 |
| **Animation** | `.gltf` 内嵌 / 独立 `.ranim` | `.ranim`（二进制） | `.ranim` | 🎯 Phase 3 |

---

## 2. 路径与目录约定

### 2.1 项目内资产路径

```
<project>/
├── assets/                  用户编辑的源资产
│   ├── shaders/
│   ├── textures/
│   ├── models/
│   ├── audio/
│   ├── fonts/
│   └── scenes/
│
└── build/<config>/
    └── assets/              构建产物（中间 + 运行时格式）
        ├── shaders/*.spv 或 *.h
        ├── textures/*.ktx2
        ├── models/*.rmesh
        └── ...
```

### 2.2 引擎内置资产

引擎自带的资产（如默认 shader、内置字体）放 `main/shaders/`、`main/builtin_assets/` 等位置。当前 Phase 1 仅有 `main/shaders/`。

### 2.3 资产虚拟路径（计划）

VFS 引入后（Phase 2，见 [F-111](../product/REQUIREMENTS.md)），代码中以 `res://path/to/asset` 形式引用：

| 协议 | 解析到 |
|------|-------|
| `res://` | 项目资产根目录 |
| `engine://` | 引擎内置资产 |
| `user://` | 用户数据目录（save / config） |

---

## 3. Shader 工作流（已实现）

### 3.1 源文件

GLSL 着色器源文件位于 `main/shaders/`（或子目录），扩展名按阶段分：

| 扩展 | Vulkan stage |
|------|-------------|
| `.vert.glsl` / `.vert` | vertex |
| `.frag.glsl` / `.frag` | fragment |
| `.comp.glsl` / `.comp` | compute |
| `.geom.glsl` / `.geom` | geometry |

### 3.2 编译为内联头文件

通过 [`misc/cmake/RoverShader.cmake`](../../misc/cmake/RoverShader.cmake) 提供的 `rover_add_shader()` 函数：

```cmake
include(RoverShader)
rover_add_shader(_shader_headers
    SHADER     "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert.glsl"
    STAGE      vert
    VAR        triangle_vert_spv
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
```

底层调用 `glslangValidator -V --vn <VAR>`，输出 C 头文件：

```c
static const uint32_t triangle_vert_spv[] = { 0x07230203, ... };
```

`#include` 该头文件后即可作为 `ShaderDesc::bytecode` 使用。

### 3.3 重编译触发

- CMake/Ninja 通过文件 mtime 自动追踪 GLSL 变化
- 强制重编：`./misc/scripts/rover-cli shaders`

### 3.4 Shader 编写约束

- 目标 Vulkan 1.3 / SPIR-V 1.6
- `layout(set = N, binding = M)` 与 C++ 端的 `BindGroupDesc`（Phase 2）保持一致
- 避免使用 GLSL 扩展（除非明确兼容所有目标后端）
- 入口固定为 `main`（与 driver 层硬编码一致）

### 3.5 跨后端 Shader（未来）

| 后端 | Shader 输入格式 | 转换工具 |
|------|---------------|---------|
| Vulkan | SPIR-V | glslangValidator（GLSL → SPIR-V） |
| D3D12 | DXIL / DXBC | dxc（HLSL → DXIL）or SPIRV-Cross（SPIR-V → HLSL → dxc） |
| Metal | MSL | SPIRV-Cross（SPIR-V → MSL） |
| WebGPU | WGSL / SPIR-V | naga（SPIR-V → WGSL） |

**初步策略**：以 GLSL → SPIR-V 为主源，通过 SPIRV-Cross 反编译到其他后端格式。具体方案待 Phase 6 实施时通过 ADR 决定。

---

## 4. Texture 工作流（计划）

### 4.1 源格式

- 编辑：`.png`（首选，无损压缩）/ `.jpg`（仅大图、有损可接受） / `.tga`（透明通道支持）
- HDR：`.exr` / `.hdr`

### 4.2 编译目标

**主推 KTX2**：

- 支持基于 Basis Universal 的 BC7 / ETC1S 转码（运行时按目标平台选码）
- 支持 mipmap 链
- 跨 GPU 后端通用（Vulkan、D3D12、Metal、WebGPU 都支持）

### 4.3 工具链

- `toktx`（KTX-Software）：`.png` → `.ktx2`
- 计划：`./misc/scripts/rover-cli import-textures`（待 Phase 2 实现）

### 4.4 元数据

每张纹理同目录可有 `.meta.json`（与 Unity 类似）：

```json
{
  "filter": "linear",
  "address_mode": "repeat",
  "srgb": true,
  "mipmaps": true,
  "anisotropy": 16
}
```

---

## 5. Model / Mesh 工作流（计划）

### 5.1 源格式

- **首选 GLTF / GLB**：开放标准，工具链成熟，支持 PBR 材质、动画、皮肤
- **次选 FBX**：受 Autodesk 私有协议限制，但兼容更广

### 5.2 中间 / 运行时格式

自定义 **`.rmesh`**（二进制）：

```
header
  magic       "RMESH"
  version     u32
  vertex_count u32
  index_count  u32
  ...
vertex_data
  per-attribute layout
index_data
materials   (引用 .rmat)
skeleton    (可选)
```

设计原则：
- 直接 mmap 即可上传 GPU，无需重排
- 头部含校验和与版本，方便升级
- 顶点 layout 可配置（顺应不同 pipeline 需求）

### 5.3 import 工具

- 基于 [tinygltf](https://github.com/syoyo/tinygltf) 或 [cgltf](https://github.com/jkuhlmann/cgltf) 解析 GLTF
- CLI 入口：`rover import <file.gltf> --out <file.rmesh>`（计划）
- 编辑器 GUI：拖拽导入 + Inspector 配置

---

## 6. Asset Registry（计划）

### 6.1 概念

每个项目维护一个 **Asset Registry**：

- UUID → 资产路径映射
- 资产元数据（导入参数、依赖）
- 双向引用追踪（Mesh A 用了 Material B；Material B 用了 Texture C）

### 6.2 文件位置

`<project>/.rover/registry.db`（SQLite 或 JSON，待选）。

### 6.3 接口

由 `modules/serialization/` 提供：

```cpp
class AssetRegistry {
public:
    AssetID load(std::string_view virtual_path);
    void unload(AssetID id);
    template<typename T> Ref<T> get(AssetID id);
};
```

---

## 7. import / export 工作流（计划）

### 7.1 来源

- 编辑器 GUI：右键导入
- 编辑器 CLI：`rover import <file>` / `rover export <asset_id>`
- 命令行批量：`rover import --all assets/`

### 7.2 触发

- 项目首次打开时扫描 `assets/` 全量导入
- 文件系统 watcher 增量重新导入（Phase 3+）
- CI 构建中预导入产物

### 7.3 元数据格式

每个源资产同目录可放 `.meta.json` 控制 import 参数。如果不存在，使用默认参数。

---

## 8. 版本控制

### 8.1 应该提交到 git 的

- ✅ 所有源资产（`assets/**`）
- ✅ `.meta.json` 元数据
- ✅ Asset Registry 数据库（如果是文本格式）
- ✅ 引擎内置资产（`main/shaders/**`、`main/builtin_assets/**`）

### 8.2 不应该提交的

- ❌ `build/<config>/assets/**`（中间产物，可重新生成）
- ❌ Asset Registry 二进制缓存
- ❌ 平台特定的导出产物（最终打包通过 CI 生成）

### 8.3 大文件

资产体积超过 5MB 时考虑使用 [git-lfs](https://git-lfs.github.com/)，配置 `.gitattributes`：

```
*.png filter=lfs diff=lfs merge=lfs -text
*.fbx filter=lfs diff=lfs merge=lfs -text
*.exr filter=lfs diff=lfs merge=lfs -text
```

---

## 9. 现状与待办

### 9.1 当前状态（v0.1）

- ✅ Shader：GLSL → SPIR-V → C 头文件，集成到 CMake
- ❌ 其他类型尚未实现

### 9.2 Phase 2 任务

- 🎯 [F-111](../product/REQUIREMENTS.md) 文件系统抽象 + Linux 实现
- 🎯 [F-406](../product/REQUIREMENTS.md) modules/serialization：基础二进制 + 文本格式
- 🎯 Texture KTX2 import 流程
- 🎯 Model GLTF import → `.rmesh`

---

## 10. 反例

```
❌ 把 Photoshop 工程文件 (.psd) 提交到 assets/
   → 体积大，工程文件含历史层信息，无法直接被引擎用
   → 应导出为 .png 后提交，.psd 单独存源文件仓库

❌ 在 C++ 代码中硬编码资产绝对路径
   → const char* path = "/home/me/proj/assets/foo.png";
   → 应使用 res://foo.png 配合 VFS

❌ 在不同后端中使用不同 shader 源
   → drivers/vulkan 用 GLSL，drivers/d3d12 用 HLSL
   → 应当从 GLSL → SPIR-V → 各后端格式（通过 SPIRV-Cross / naga）

❌ 修改 vendor/ 中的 stb_image / tinygltf 源码
   → 通过 wrapper 在 vendor/CMakeLists.txt 中加 patch，或写自己的 wrapper layer
```

---

## 11. 参考

- [`docs/dev/MISC.md`](../dev/MISC.md) §9 RoverShader.cmake
- [ADR-0006 IO 拆分](../product/adr/ADR-0006-io-split-platform-and-serialization.md)
- [`docs/product/REQUIREMENTS.md`](../product/REQUIREMENTS.md) §1.1 / §1.5 资产相关需求
- [KTX 2.0](https://www.khronos.org/ktx/)
- [glTF 2.0](https://www.khronos.org/gltf/)
- [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross)
- [Basis Universal](https://github.com/BinomialLLC/basis_universal)
