# ADR-0013: Frame Graph 单生产者依赖编译与执行计划

- **Status**: Accepted
- **Date**: 2026-11-05
- **Deciders**: core team

**后续决策**：2026-11-07 的 [ADR-0014](ADR-0014-resource-barriers.md) 在本文的单生产者和依赖排序契约上增加资源用途、自动 barrier 与导出同步。[ADR-0016](ADR-0016-frame-graph-resource-versions.md) 随后引入显式版本与物理覆盖依赖，每个逻辑版本保持单生产者。下文记录 2026-11-05 的编译方案。

## Context

FrameGraph 原先只校验资源编号并按声明顺序执行，不能处理后声明的生产者，也没有循环检测。现有资源 ID 没有版本或子资源信息，无法区分读取初始值、读取某次写入结果，以及多次覆盖之间的关系。

## Decision

本轮每个资源 ID 只允许一个写入 pass。所有读取依赖该生产者，与声明顺序无关；没有生产者的导入资源是外部输入。同 pass 读写、多个 pass 写入同一资源、重复导入同一物理资源均拒绝，避免歧义或别名绕过依赖。颜色附件隐式声明一次写入。重复声明和重复依赖边去重。

使用生产者到消费者的有向图和 Kahn 拓扑排序；每步选注册编号最小的就绪 pass，保留全部 pass。注册编号与 pass 数组保持稳定，另存执行顺序和各 pass 的前置依赖供检查。编译失败清空计划；资源、pass、访问声明、附件变化使计划失效。执行期间拒绝修改图和重入编译/执行；打开命令列表失败后停止本次图执行，避免继续运行依赖未产出数据的 pass。

资源描述提供只读访问，声明接口统一处理计划失效。该阶段编译负责 CPU 依赖和执行顺序，跨 pass GPU 同步与布局转换由调用方及现有驱动路径负责，GPU 资源由调用方分配。

## Consequences

- 可以先声明消费者，编译得到确定的执行顺序；错误不会执行部分拓扑计划。
- 一个资源的多个写入阶段需要未来显式资源版本机制；不能把同一 GPU 资源以多个 ID 导入来模拟版本。
- 当前采用整个资源粒度、单队列串行记录，不做 pass 剔除、瞬态分配、子资源跟踪或自动 barrier。
- FrameGraph 由单线程构建和执行；执行期检查用于阻止重入与修改。

## Alternatives Considered

- 按声明顺序推导 RAW/WAR/WAW：适合原地多次读写，但不能表达本轮所需的后声明生产者；没有额外依赖 API 时边全部向后，也不能形成需检测的循环。
- 立即引入资源版本、访问阶段与子资源：长期需要，但与本轮 CPU 依赖编译分开实施，避免把 GPU 同步接口一并扩大。

## References

- [Frame Graph 使用契约](../../dev/FRAME_GRAPH.md)
- [实现](../../../services/graphics/frame_graph.cpp)
- [测试](../../../tests/services/frame_graph_test.cpp)
- 本地参考 `../tj/Packages/com.unity.render-pipelines.core/Runtime/RenderGraph/RenderGraph.cs` 的 `CountReferences`：分离资源生产者/消费者记录。tj 支持多生产者，其完整语义不等同于本决策。
- 本地参考 `../godot/servers/rendering/rendering_device_graph.cpp` 的 `_add_command_to_graph` 与命令排序：资源跟踪建立邻接关系，再安排执行。Godot 还追踪访问状态、别名与子资源；本轮未移植其同步和排序策略。
