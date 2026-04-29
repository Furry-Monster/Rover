# ADR-0007: Shader 源语言选 Slang / HLSL + 转译器架构

- **Status**: Accepted
- **Date**: 2026-04-29
- **Deciders**: core team

## Context

Phase 1 当前以 GLSL 为 shader 源语言，通过 `glslangValidator` 编译为 SPIR-V 后内联为 C 头文件（见 [`docs/standard/ASSETS.md`](../../standard/ASSETS.md) §3 与 [`misc/cmake/RoverShader.cmake`](../../../misc/cmake/RoverShader.cmake)）。Phase 1 这一选择是「最短路径走通三角形」，但放眼跨后端长期演进存在明显问题：

- **生态规模**：现代游戏 / 引擎产业（含 AAA 与 indie）shader 主流语言是 HLSL；DXC 已是 Khronos 官方推荐的 SPIR-V 编译器之一。GLSL 在 Vulkan 之外的后端（D3D12、Metal、Web）需要二次反编译才能落地。
- **特性集**：HLSL 拥有更现代的语言特性（template、`static const`、丰富的 intrinsics、resource binding 元数据）；GLSL 的扩展机制零散，跨编译器兼容性差。
- **Slang 的优势**：Slang 是 NVIDIA 主导、Khronos 接管、HLSL 兼容超集。具备：模块化（`import`）、泛型、自动微分、capability-based feature 切换，并且 `slangc` 原生输出 SPIR-V / DXIL / Metal / WGSL / GLSL / HLSL，**单一前端、多目标后端**，避免了 SPIRV-Cross 链式反编译的精度损失。
- **DCC 与外部工具链**：HLSL 是 Unity HLSL / Unreal HLSL / D3D Shader Compiler / Slang / DXC / FXC 的事实通用语言；Substance / Marmoset / RenderDoc 等都可以直接消费 HLSL/Slang 源码。
- **跨后端策略与 GLSL → SPIR-V → SPIRV-Cross → MSL/HLSL 的链条相比**，Slang 在多目标场景下生成结果更直接、性能更可预测。

Phase 1 用 GLSL 的历史负担可控（当前只有 `triangle.vert.glsl` / `triangle.frag.glsl` 两个 shader），现在切换迁移成本低。

## Decision

**Rover 引擎统一以 Slang（首选）/ HLSL（兼容，作为 Slang 的子集）作为 shader 源语言，所有 GPU 后端通过转译器（`slangc` 为主，`dxc` 为副）从单一源生成目标字节码。**

具体规则：

1. **源格式**：扩展名 `.slang`（推荐）或 `.hlsl`（HLSL 风格仍可被 `slangc` 编译）。
2. **主转译器**：`slangc`（来自 [shader-slang/slang](https://github.com/shader-slang/slang)）。
3. **目标矩阵**：

   | 后端 | 目标格式 | 工具 |
   | ----- | --------- | ----- |
   | Vulkan | SPIR-V 1.6 | `slangc -target spirv` |
   | D3D12 | DXIL | `slangc -target dxil`（或 `dxc`） |
   | Metal | MSL | `slangc -target metal` |
   | WebGPU | WGSL | `slangc -target wgsl`（视成熟度而定，必要时走 SPIR-V → naga → WGSL 兜底） |

4. **构建集成**：`misc/cmake/RoverShader.cmake` 的 `rover_add_shader()` 切换底层调用从 `glslangValidator` 改为 `slangc`；产物（C 头文件 / `.spv` / 其他后端字节码）格式与 API 不变。
5. **入口约定**：保持入口名 `main`，与现有 `ShaderDesc::entry_point` 默认值兼容。
6. **资源绑定**：Slang 的 `[[vk::binding(M, N)]]` / `register(t0, space1)` 注解须与 C++ 端 `BindGroupDesc`（Phase 2 抽象）保持一致；提供脚本/编译期校验避免漂移。
7. **过渡计划**：Phase 1 现有 GLSL 文件随 Phase 2 切换 `slangc` 时一同重写为 `.slang`；不并行维护 GLSL + Slang 两套源。

## Consequences

**Positive:**

- **单一源 → 多后端**：减少二次反编译链路，避免 SPIRV-Cross 在 MSL/HLSL 上偶发的语义偏差。
- **生态对齐**：HLSL 是 D3D / Unity / Unreal 的母语，新贡献者上手成本低，外部工具链（RenderDoc / Nsight / PIX）显示更友好。
- **未来可用 Slang 高级特性**（模块、泛型、reflection API），便于实现 uber-shader / 自动 PBR 模板。
- **删去 GLSL → SPIR-V → SPIRV-Cross 兜底路径**：Phase 6 跨后端时少一个依赖（SPIRV-Cross 仅作可选回退）。

**Negative / Trade-offs:**

- **新增 vendor 依赖 `slang`**：源码包体积非平凡（数十 MB），需通过 ADR-0008 的手动下载流程引入到 `vendor/slang/`。
- **Slang 自身仍在演进**：少数边缘特性可能不稳定；遇到 bug 时需要 fork 或上游 PR。
- **Phase 1 现有 GLSL 需要重写**：成本可控（两个文件），但要纳入 Phase 2 任务清单。
- **CI 构建时间略增**：`slangc` 比 `glslangValidator` 略重；可通过缓存与并行编译缓解。
- **不再支持 GLSL 源**：习惯 GLSL 的贡献者需要切换；项目用户编写自定义 shader 时需写 Slang/HLSL（文档需补一份迁移指南）。

## Alternatives Considered

- **维持 GLSL 主源 + SPIRV-Cross 反编译到其他后端**：链路长、生成代码可读性差、Metal/HLSL 偶发语义问题。放弃。
- **HLSL（DXC 直出）作为主源，不引入 Slang**：可行，但放弃了 Slang 的模块化与多目标直出能力；将来若想用 Slang 还得再迁移一次。Slang 与 HLSL 兼容，等于 HLSL 的超集 + 多后端能力，无理由不取上限。放弃 HLSL-only。
- **WGSL 作为主源**：W3C 规范清晰，但语言特性最少（无模板 / 模块），且 WGSL → 其他后端的转译生态尚未成熟。放弃。
- **Pure SPIR-V 手写**：仅适合极少数 lib/intrinsic 场景，不适合主流 shader 编写。放弃。

## References

- [shader-slang/slang](https://github.com/shader-slang/slang)
- [DirectX Shader Compiler (dxc)](https://github.com/microsoft/DirectXShaderCompiler)
- [`docs/standard/ASSETS.md`](../../standard/ASSETS.md) §3 Shader 工作流
- [`misc/cmake/RoverShader.cmake`](../../../misc/cmake/RoverShader.cmake)
- [`docs/product/REQUIREMENTS.md`](../REQUIREMENTS.md) F-006 SPIR-V shader 编译时内联
- ADR-0008 vendor 手动下载政策（决定 `slang` 如何进入 vendor/）
