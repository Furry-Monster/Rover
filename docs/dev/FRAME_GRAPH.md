# Frame Graph 依赖编译

`services/graphics/FrameGraph` 组织一帧的 pass。`compile()` 在 CPU 上验证声明、建立依赖 DAG、生成确定性拓扑执行计划，`execute()` 按计划记录命令。架构决策见 [ADR-0013](../product/adr/ADR-0013-frame-graph-compilation.md)。

## 声明资源与 pass

资源由调用方创建并导入，每个物理纹理/缓冲只导入一次。不同类型可以使用相同数值的句柄。每个资源最多一个生产者；`read(id)` 消费该生产者的输出，不取决于 pass 注册先后。无生产者的资源作为外部输入，由调用方保证其内容与同步有效。

```cpp
FrameGraph graph;
auto input = graph.import_buffer("input", input_buffer);
auto intermediate = graph.import_buffer("intermediate", intermediate_buffer);
auto output = graph.import_buffer("output", output_buffer);

// 可以先注册消费者。execute 回调仍需记录实际 GPU 命令。
graph.add_pass("consume", [&](PassBuilder& b) {
    b.read(intermediate);
    b.write(output);
}, consume_commands);
graph.add_pass("produce", [&](PassBuilder& b) {
    b.read(input);
    b.write(intermediate);
}, produce_commands);

if (graph.compile())
{
    // execution_order() == {1, 0}; dependencies()[0] == {1}
    graph.execute(device);
}
```

`set_color_attachment(pass, texture, clear)` 隐式声明该纹理写入，并自动包围 render pass。也可显式 `write(texture)`，重复声明去重。附件必须是带有效 framebuffer 的导入纹理；重设附件会替换原隐式写入，显式声明仍保留。framebuffer 附带的深度附件目前没有单独跟踪；需要排序的其他纹理/缓冲须显式声明。调用方负责 viewport、scissor 和实际绘制/计算命令。

## 编译与错误

- 每一步选择注册编号最小的就绪 pass。没有依赖的 pass 也会保留；不做剔除。
- `passes()` 始终按注册顺序，pass ID 不随排序变化；`execution_order()` 是执行编号序列，`dependencies()[id]` 是该 pass 去重并排序后的前置 pass 编号。
- 非法资源编号（包括 `INVALID_RESOURCE_ID`）、无效导入句柄、重复物理资源导入、无效附件、多个生产者、同 pass 读写、循环依赖均记录日志并使 `compile()` 返回 `false`。循环诊断列出被循环阻塞的 pass，可能包括其下游。
- 编译失败或图变化后，执行顺序和依赖列表为空，`execute()` 不记录命令。所有声明完成后重新调用 `compile()`。
- 无效 pass 编号的声明会使当前图无效，`reset()` 后可重新构建；有效 pass 的错误资源声明也可通过重新构建修正。

资源描述通过 `resource(id)` 只读访问，禁止直接修改内部数据绕过计划失效。`PassBuilder` 只用于当前图构建阶段，不得在 `reset()` 后复用。构建与执行都由单线程调用，执行回调期间修改图、编译或重入执行会被拒绝。每个 pass 打开独立命令列表；若打开失败，本次执行停止，已记录的命令不会回滚。调用方负责 `begin_frame()`、`end_frame()` 与 `present()`，并保证导入资源的生命周期。

## 当前边界与验证

这里的依赖只保证 CPU 记录顺序，尚未生成跨 pass GPU barrier 或纹理布局转换。不能仅凭拓扑排序就假设渲染到纹理后采样已经正确同步。多写入需要后续资源版本设计；瞬态分配、异步队列、pass 剔除与子资源访问也尚未实现。

`tests/services/frame_graph_test.cpp` 使用 mock 设备验证逆序注册、链/分叉/汇合、确定性顺序、错误图、计划失效、附件依赖及执行失败边界。运行 `ctest --test-dir build --output-on-failure`。
