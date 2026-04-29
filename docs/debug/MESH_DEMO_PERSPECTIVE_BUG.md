# Mesh Demo：透视「近小远大」错误

**日期**：2026-04-29  
**影响范围**：Phase 2 `mesh_demo` 立方体渲染；`core/math` 透视矩阵与视口组合约定  
**状态**：已修复  

---

## 现象

使用默认相场景渲染立方体时，透视关系异常，呈现 **近处小、远处大**（反向收缩），与正确透视相反。

---

## 原因（摘要）

`Mat4::perspective` 原先在 **Y 分量**上使用 **OpenGL 教科书式** 的 `+1/tan(fov/2)`（clip 空间 Y 与经典 GL NDC「向上」一致)，同时在 `mesh_demo` 里用 **负的 viewport `height`** 把 NDC 翻成 Vulkan 习惯（Y 向下）。

两套约定在理论上可以配对，但在当前管线里容易与 **透视除法 `w`**、深度与光栅化预期混在一起，表现为明显的透视反转。更稳妥的做法是采用与现代 API（Vulkan / Metal / D3D12）一致的 **单一 canonical clip**：在投影矩阵内直接 **翻转 Y**（`c[1][1] = -1/tan(fov/2)`），并使用 **正的** 全屏视口高度；正面绕序改为 **CCW + Back cull**，与正高度视口下的三角形 winding 一致。

**Z 与 w**：实现里 `c[2][3] = -1`（`clip.w = -z_view`）以及将 `[near,far]` 映射到 **[0,1]** 的 `c[2][2]` / `c[3][2]` 本身已是 Vulkan 深度区间，**未**作为本 bug 主因修改。

---

## 修复（落地代码）

| 位置 | 变更 |
|------|------|
| `core/math/mat4.cpp` | `perspective`：`c[1][1]` 取 **负**的 `1/tan(fov/2)` |
| `main/mesh_demo.cpp` | 视口：`y = 0`，`height = +fb_height`（不再为负） |
| `main/mesh_demo.cpp` | 管线：`FrontFace::CounterClockwise`（替代为配合负视口用的 CW） |

约定说明见 `core/math/mat4.h` 中 `perspective` / `ortho` 注释：**引擎 canonical clip** 与现代 API 家族对齐；若日后接经典 OpenGL 后端，应在驱动层做 clip 修正，而非在 `core/math` 按 API 分支多套矩阵。

---

## 验证

- 本地运行 mesh demo：立方体透视正常（近大远小）。
- `rover_tests` 全量通过（数学侧无针对透视矩阵数值的硬编码断言）。

---

## 参考

- RenderDoc _CAPTURE（`.rdc`）可用于核对 `CameraUbo` 的 `projection`、`view`、视口与单次 draw 的 clip 输出（工程内曾有 `.rdc/cube.rdc` 类捕获）。
