# Mesh Demo：透视「近小远大」错误

**日期**：2026-04-29  
**影响范围**：Phase 2 `mesh_demo` 立方体渲染；`core/math` 透视矩阵与视口组合约定  
**状态**：已修复  

---

## 现象

默认场景中的立方体呈现近处小、远处大的反向透视。

---

## 投影与视口约定

旧路径在 `Mat4::perspective` 中使用正的 Y 比例 `+1/tan(fov/2)`，并在 `mesh_demo` 中通过负 viewport `height` 翻转 Y。

修复统一采用 Rover 的 canonical clip 约定：投影矩阵翻转 Y（`c[1][1] = -1/tan(fov/2)`），视口使用正高度，管线使用 CCW 正面绕序与 Back cull。

**Z 与 w** 保持原实现：`c[2][3] = -1` 对应 `clip.w = -z_view`；`c[2][2]` / `c[3][2]` 将 `[near,far]` 映射到 `[0,1]`。

---

## 修复（落地代码）

| 位置 | 变更 |
|------|------|
| `core/math/mat4.cpp` | `perspective`：`c[1][1]` 取 **负**的 `1/tan(fov/2)` |
| `main/mesh_demo.cpp` | 视口：`y = 0`，`height = +fb_height`（不再为负） |
| `main/mesh_demo.cpp` | 管线：`FrontFace::CounterClockwise`（替代为配合负视口用的 CW） |

约定说明见 `core/math/mat4.h` 中 `perspective` / `ortho` 注释。后续接入经典 OpenGL 后端时，在驱动层转换 clip 约定，`core/math` 继续使用统一矩阵定义。

---

## 验证

- 本地运行 mesh demo：立方体透视正常（近大远小）。
- `rover_tests` 全量通过（数学侧无针对透视矩阵数值的硬编码断言）。

---

## 2026-11-05：正交 demo 回归检查

新增 `main/builtin_assets/scenes/demos/orthographic.rscene` 时发现 `Mat4::ortho` 仍使用正的 Y 比例，未遵守透视投影的 canonical clip 约定。同一正高度 viewport 与剔除管线切换到正交相机会反转画面上下方向和三角形绕序。修复 `c[1][1] = -2/(top-bottom)`，同时令 `c[3][1] = +(top+bottom)/(top-bottom)`，支持非对称边界。`tests/core/math_test.cpp` 验证非对称视锥的四角、中心、近远平面与透视投影相同的 Y 方向。

球体生成器的索引顺序也已改为与外法线一致；`tests/modules/mesh_texture_test.cpp` 逐个检查非退化三角形的叉积与顶点法线方向，覆盖 cube、quad、sphere。

## 参考

- RenderDoc _CAPTURE（`.rdc`）可用于核对 `CameraUbo` 的 `projection`、`view`、视口与单次 draw 的 clip 输出（工程内曾有 `.rdc/cube.rdc` 类捕获）。
