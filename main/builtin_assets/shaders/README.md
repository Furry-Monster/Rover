# 内置 shader

源文件采用 Slang，由仓库内的 `slangc` 编译为 Vulkan 1.3 / SPIR-V 1.6，再嵌入 C++ 头文件。构建规则见 [RoverShader.cmake](../../../misc/cmake/RoverShader.cmake)，编译器来源见 [Slang 记录](../../../vendor/SLANG.md)。

| 文件 | 源入口 | 用途 |
|------|--------|------|
| `triangle.slang` | `vertex_main`、`fragment_main` | 三角形回归示例 |
| `mesh.slang` | `vertex_main`、`fragment_main` | 相机、世界矩阵、平行光与固定材质的 HDR 场景 |
| `post_process.slang` | `vertex_main`、`tone_map_main`、`present_main` | 全屏三角形、曝光与 Reinhard 映射、交换链呈现 |

`rover_add_shader(... ENTRY ...)` 选择源入口，SPIR-V 入口统一为 `main`。矩阵采用列主序，使用 `mul(matrix, column_vector)`，匹配 C++ `Mat4`。相机投影已处理 Vulkan Y 方向，shader 直接使用该矩阵。

mesh 的 set 0 binding 0 为两个 mat4 的相机 UBO；push constant 为世界矩阵、光方向、光颜色与环境强度、albedo，共 112 字节。顶点读取 position 和 normal，沿用 32 字节 MeshVertex 步长。

后处理的 set 0 binding 0 为独立 SampledTexture，binding 1 为独立 Sampler；16 字节 push constant 包含曝光、色调映射开关和填充。HDR 纹理为 RGBA16F，映射结果为线性 RGBA8，呈现至 sRGB 附件时由格式完成编码。全屏三角形使用原始 `SV_VulkanVertexID`，绘制的 first_vertex 固定为 0，无需 BaseVertex 功能。

运行与 GPU 回读验证见 [离屏后处理](../../../docs/dev/POST_PROCESS.md)。
