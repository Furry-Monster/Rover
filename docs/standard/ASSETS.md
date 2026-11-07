# 资源处理工作流

本规范说明资源格式、路径、版本控制和 import/export 工作流。下表区分当前数据路径与目标文件格式，现有能力和后续工作集中列在 §9。

---

## 1. 资源类型与格式


| 资源类别 | 源格式与目标格式 | 运行时路径 | 实现与规划 |
|----------|------------------|------------|------------|
| **Shader** | Slang `.slang` / HLSL `.hlsl` → SPIR-V | 本地构建的 Slang 编译器生成内联 C++ 头文件 | Vulkan 已实现，其他后端按 ADR-0007 推进 |
| **Texture** | 目标源格式为 PNG / JPG / TGA / EXR，中间格式为 KTX2 / DDS / `.rtex` | 当前由内存纹理数据上传 GPU | 上传已实现，外部文件导入待完成 |
| **Model / Mesh** | 目标源格式为 GLTF / GLB，FBX 待评估；中间格式为 `.rmesh` | 当前生成内置网格并上传 GPU | 内置 cube / quad / sphere 已实现，外部模型导入待完成 |
| **Audio** | WAV / OGG / MP3 | 直接读取短音、流式读取长音，解码为 PCM 后混音 | Phase 4 规划 |
| **Font** | TTF / OTF → MSDF atlas 或 bitmap atlas | 纹理与 glyph 元数据 | Phase 3 规划 |
| **Scene** | `.rscene` JSON 与 RBIN 二进制 | 加载 World 并恢复资源引用 | 已实现，见 [场景加载](../dev/SCENE_LOADING.md) |
| **Material** | 目标为 `.rmat` 文本资源 | demo 当前使用固定材质 | 材质资源工作流待完成 |
| **Animation** | GLTF 内嵌或 `.ranim` 二进制 | 解析动画资源 | Phase 3 规划 |


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

[`main/builtin_assets/`](../../main/builtin_assets/README.md) 是引擎内置资源的统一存放位置，按资源类型组织。场景放在 `scenes/` 下，示例场景集中于 `scenes/demos/`；后续模型、纹理、材质、字体、音频分别放入 `models/`、`textures/`、`materials/`、`fonts/`、`audio/`，在加入对应资源时创建目录。

当前六个 Phase 2 示例场景通过 `--scene main/builtin_assets/scenes/demos/<name>.rscene` 加载，见 [运行说明](../../main/builtin_assets/scenes/demos/README.md)。

当前参与编译的 shader 源文件位于 `main/builtin_assets/shaders/`，其编译流程见下文。

### 2.3 资产虚拟路径（计划）

Phase 2 的 [F-111](../product/REQUIREMENTS.md) 已提供 FileAccess/DirAccess 与 Linux 文件系统实现，场景使用实际文件路径，内置网格使用注册的稳定路径。资产虚拟路径解析随 Phase 3 的资产管线建设，目标协议为：


| 协议          | 解析到                   |
| ----------- | --------------------- |
| `res://`    | 项目资产根目录               |
| `engine://` | 引擎内置资产                |
| `user://`   | 用户数据目录（save / config） |


---

## 3. Shader 工作流

> 架构决策：见 [ADR-0007](../product/adr/ADR-0007-shader-source-language.md)。**Slang（首选）/ HLSL** 作为唯一源语言，通过 `slangc`（主）/ `dxc`（副）转译到目标后端字节码。GLSL 不再作为合法源格式。

### 3.1 源文件

Slang/HLSL 着色器源文件位于 `main/builtin_assets/shaders/`（或子目录）：


| 扩展        | 含义                                         |
| --------- | ------------------------------------------ |
| `.slang`  | **首选**：Slang 源（HLSL 兼容超集，可声明多入口、模块、泛型）       |
| `.hlsl`   | 兼容：纯 HLSL 源（也由 `slangc` 编译，等价于 Slang 的 HLSL 模式） |

Stage 通过文件内的 `[shader("vertex")]` / `[shader("fragment")]` / `[shader("compute")]` 注解或 `slangc -stage <stage>` 显式声明，**不再**通过 `.vert` / `.frag` 后缀绑定。

### 3.2 编译为内联头文件

通过 [`misc/cmake/RoverShader.cmake`](../../misc/cmake/RoverShader.cmake) 提供的 `rover_add_shader()` 函数：

```cmake
include(RoverShader)
rover_add_shader(_shader_headers
    SHADER     "${CMAKE_CURRENT_SOURCE_DIR}/builtin_assets/shaders/triangle.slang"
    STAGE      vertex          # vertex / fragment / compute / geometry
    ENTRY      vertex_main     # 入口点（默认 main）
    TARGET     spirv           # 当前支持 spirv
    VAR        triangle_vs_spv
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
```

`vendor/CMakeLists.txt` 构建宿主编译器 `slangc`，shader 规则通过 target 路径调用它。编译参数为 `-target spirv -profile spirv_1_6 -stage vertex -entry vertex_main -matrix-layout-column-major -emit-spirv-directly -O0`，再由 `misc/scripts/spirv_to_header.py` 将 `.spv` 转为 C++ 头文件：

```cpp
static const std::uint32_t triangle_vs_spv[] = { 0x07230203, ... };
```

`#include` 该头文件后即可作为 `ShaderDesc::bytecode` 使用。DXIL / MSL / WGSL 输出将在对应后端接入时扩展。当前直接生成 SPIR-V，`-O0` 使构建链无需额外优化插件；编译器来源与依赖见 [依赖记录](DEPENDENCIES.md#slang-与-vulkan-headers-构建)。

### 3.3 重编译触发

- CMake/Ninja 通过文件 mtime 自动追踪 `.slang` / `.hlsl` 变化
- 强制重编：`./misc/scripts/rover-cli shaders`
- Slang 模块（`import` 引入的 `.slang` 文件）通过 `slangc -depfile` 自动登记为依赖，避免漏编

### 3.4 Shader 编写约束

- **目标 Vulkan 1.3 / SPIR-V 1.6**（Phase 1 默认后端）；其他后端字节码版本由对应 driver 决定
- `ENTRY` 选择源入口；Slang 输出的 SPIR-V 入口统一为 `main`，driver 使用 `ShaderDesc::entry_point = "main"`。生成头文件前校验入口数量与名称
- 资源绑定使用 Slang/HLSL 风格注解，**两套语义都允许**：
  - Vulkan 风格：`[[vk::binding(M, N)]]`（M = binding，N = set）
  - D3D 风格：`register(t0, space1)`（slangc 会自动映射到 SPIR-V binding）
- 矩阵使用列主序与列向量乘法，匹配 `Mat4`。全屏三角形使用 `SV_VulkanVertexID`，first_vertex 为 0
- 绑定槽位必须与 C++ 端 `BindGroupDesc`（Phase 2 抽象）保持一致；后续提供脚本/编译期校验
- 避免使用未在所有目标后端可用的高级特性（subgroup ops、bindless 等）；如必须使用，通过 Slang 的 capability 机制声明并在 ADR 中记录后端覆盖
- 不允许在源里 `#include` 未在 `vendor/slang/` / `main/builtin_assets/shaders/` 范围内的头文件

### 3.5 跨后端转译矩阵


| 后端     | 目标格式      | 主转译器                                | 备注                                              |
| ------ | --------- | ----------------------------------- | ----------------------------------------------- |
| Vulkan | SPIR-V    | `slangc -target spirv`              | Phase 2 已接入 Slang                                       |
| D3D12  | DXIL      | `slangc -target dxil` / `dxc`       | Phase 6                                          |
| Metal  | MSL       | `slangc -target metal`              | Phase 6（macOS / iOS）                              |
| WebGPU | WGSL      | `slangc -target wgsl`               | Phase 6（成熟度若不足，回退 SPIR-V → naga → WGSL）           |

**SPIRV-Cross 仅作为可选回退**（如某后端 Slang 输出有 bug），不作为主链路。

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
- 计划：`./misc/scripts/rover-cli import-textures`（Phase 3 资产导入）

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

自定义 `**.rmesh`**（二进制）：

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
- ✅ 引擎资源（`main/builtin_assets/**`）

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

### 9.1 当前状态

- Shader 通过本地 Slang → SPIR-V → C++ 头文件进入构建，三角形、网格和后处理共用该链路，见 [ADR-0015](../product/adr/ADR-0015-offscreen-post-process.md)。
- 场景使用 JSON/RBIN 序列化，内置 cube、quad、sphere 通过稳定路径恢复 GPU 缓冲，见 [场景加载](../dev/SCENE_LOADING.md)。
- 网格生成、网格/纹理上传与内存 Asset Registry 位于 `modules/serialization/`。
- 引擎内置资源按类型组织在 `main/builtin_assets/`，示例场景位于 `scenes/demos/`。

### 9.2 后续工作

- 🎯 扩展 [F-406](../product/REQUIREMENTS.md)：外部资源 import/export 与磁盘资产注册表
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
   → 应当只用 Slang/HLSL 单一源，通过 slangc 转译到各后端字节码

❌ 用 GLSL（.vert.glsl / .frag.glsl）写新 shader
   → ADR-0007 已切到 Slang/HLSL；不再接受 GLSL 源进仓库

❌ 在 .slang 里硬编码不同后端的 binding（vk vs hlsl 风格混写未声明语义等价）
   → 选一种风格写满整个 shader；slangc 会做语义映射，避免再人肉转换

❌ 修改 vendor/ 中的 stb_image / tinygltf 源码
   → 通过 wrapper 在 vendor/CMakeLists.txt 中加 patch，或写自己的 wrapper layer
```

---

## 11. 参考

- [`docs/dev/MISC.md`](../dev/MISC.md) §9 RoverShader.cmake
- [ADR-0006 IO 拆分](../product/adr/ADR-0006-io-split-platform-and-serialization.md)
- [ADR-0007 Shader 源语言选 Slang/HLSL + 转译器](../product/adr/ADR-0007-shader-source-language.md)
- [ADR-0008 vendor 手动下载](../product/adr/ADR-0008-vendor-manual-fetch.md)
- [`docs/product/REQUIREMENTS.md`](../product/REQUIREMENTS.md) §1.1 / §1.5 资产相关需求
- [Slang Shading Language](https://github.com/shader-slang/slang)
- [DirectX Shader Compiler (dxc)](https://github.com/microsoft/DirectXShaderCompiler)
- [KTX 2.0](https://www.khronos.org/ktx/)
- [glTF 2.0](https://www.khronos.org/gltf/)
- [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross)（仅作为回退路径）
- [Basis Universal](https://github.com/BinomialLLC/basis_universal)
