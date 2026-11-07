# ADR-0016: Frame Graph 资源版本与覆盖依赖

- **Status**: Accepted
- **Date**: 2026-11-07
- **Deciders**: core team

## Context

Frame Graph 当前允许每个导入资源有一个生产者。后处理链已验证实际采样；继续复用颜色目标或缓冲需要表达多次写入，以及覆盖前仍需完成的读取。

## Decision

新增 `FrameGraph::create_version(previous, name)`，返回同一物理资源的下一个逻辑版本。版本共享纹理、缓冲和 framebuffer；资源仍由调用方持有。每个版本保留单生产者约束，后继版本必须有生产者。原有 import/read/write 接口保持语义：读取所声明版本的生产者输出，无生产者的导入版本表示外部内容。

物理资源的版本形成线性链。编译器建立三类依赖：

- RAW：版本的生产者先于该版本的读取者。
- WAW：前一版本的生产者先于后一版本的生产者。
- WAR：前一版本的所有读取者先于后一版本的生产者。

版本关系显式声明，pass 可以按任意顺序注册。就绪 pass 继续按注册编号排序。分叉版本、后继版本缺少生产者、同 pass 使用同一物理资源的多个版本，以及重复导入同一物理句柄均报错。版本提供覆盖顺序，不保存历史内容，也不提供同 pass 采样与附件写入的反馈能力。

barrier 状态按物理资源累计，版本之间保持连续访问状态。导出状态只允许声明在最后一个版本；旧版本内容若需保留，由 pass 在覆盖前复制到独立资源。

本步复用导入资源和现有整资源同步接口。瞬态资源分配、显存别名、mip/layer 跟踪在后续扩展。

## Consequences

- 同一物理纹理或缓冲可以按显式版本链多次写入，覆盖前的读取由图编译器排序。
- 已有单次写入图保持接口与执行顺序；不通过重复 import 隐藏物理关系。
- GPU demo 在同一帧分别回读覆盖前后的颜色，检查 RAW、WAR、WAW 与跨帧状态。

## References

- [ADR-0013](ADR-0013-frame-graph-compilation.md)、[ADR-0014](ADR-0014-resource-barriers.md)
- [Frame Graph](../../dev/FRAME_GRAPH.md)、[离屏后处理](../../dev/POST_PROCESS.md)
