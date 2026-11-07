# Frame Graph 依赖编译与自动同步

`services/graphics/FrameGraph` 组织一帧的 pass。`compile()` 在 CPU 上验证声明、建立依赖 DAG，并生成确定性执行顺序与 barrier 计划；`execute()` 在每个 pass 之前转换其资源用途，最后转换导出资源。架构决策见 [ADR-0013](../product/adr/ADR-0013-frame-graph-compilation.md) 、[ADR-0014](../product/adr/ADR-0014-resource-barriers.md) 和 [ADR-0016](../product/adr/ADR-0016-frame-graph-resource-versions.md)。

## 声明资源与 pass

调用方创建并导入资源，每个物理纹理或缓冲导入一次，后续写入通过 `create_version(previous, name)` 声明逻辑版本。纹理和缓冲使用独立的句柄命名空间，数值可以相同。每个版本最多一个生产者；`read(id)` 消费该版本生产者的输出。没有生产者的导入版本表示已初始化的外部内容，后继版本必须有生产者。

`read(id)` 默认为 `ShaderRead`。`write(id)` 默认为 `Automatic`：如果资源是本 pass 的颜色附件，则推断为 `ColorAttachment`，否则推断为 `StorageWrite`。显式声明用途时不会被附件覆盖；一个资源在同一 pass 内只能有一种用途。

| 用途 | 适用资源 | 声明方式 |
|---|---|---|
| `ShaderRead` / `StorageRead` | 采样纹理 / storage 纹理或缓冲 | `read(id, usage)` |
| `UniformRead` / `VertexRead` / `IndexRead` / `IndirectRead` | 缓冲 | `read(id, usage)` |
| `StorageWrite` | storage 纹理或缓冲 | `write(id, usage)` |
| `ColorAttachment` / `DepthStencilAttachment` | 对应格式的附件纹理 | `write(id, usage)`；颜色附件也可隐式声明 |
| `TransferRead` / `TransferWrite` | 纹理或缓冲 | `read` / `write` 指定对应用途 |
| `HostRead` / `HostWrite` | CPU 可访问缓冲 | `set_final_access(id, usage)` |
| `Present` | 交换链纹理 | 驱动在 `end_frame()` 中自动转换，也可显式导出 |

资源创建 flags 必须允许声明的用途。编译器验证图中的类型、格式和读写方向；驱动录制 barrier 时验证真实资源 flags。

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
    // barrier 在 produce / consume 的命令列表中、执行回调之前记录。
    graph.execute(device);
}
```

`set_color_attachment(pass, texture, clear)` 隐式声明一次 `ColorAttachment` 写入，并在执行回调前后打开、关闭 render pass。附件使用带有效 framebuffer 的颜色纹理。重设附件会替换原隐式写入，显式 `write(texture)` 声明继续保留；重复声明去重。

参与图中排序的深度附件通过 `write(depth, ResourceAccess::DepthStencilAttachment)` 声明，并与实际 framebuffer 匹配。驱动转换 framebuffer 的全部实际附件，也支持直接使用设备接口的调用方。viewport、scissor 和实际绘制/计算命令由执行回调设置。

## 同一物理资源的多次写入

```cpp
auto original = graph.import_buffer("data.v0", buffer_handle);
auto updated = graph.create_version(original, "data.v1");

// 覆盖 pass 可以先登记；图会等待旧版本的全部读取者。
graph.add_pass("overwrite", [&](PassBuilder& b) {
    b.write(updated, ResourceAccess::TransferWrite);
}, overwrite_commands);
graph.add_pass("read old", [&](PassBuilder& b) {
    b.read(original, ResourceAccess::TransferRead);
}, copy_old_commands);
graph.add_pass("initialize", [&](PassBuilder& b) {
    b.write(original, ResourceAccess::TransferWrite);
}, initialize_commands);
graph.add_pass("read new", [&](PassBuilder& b) {
    b.read(updated, ResourceAccess::TransferRead);
}, copy_new_commands);
// 编译顺序：initialize → read old → overwrite → read new。
```

编译器为版本建立以下关系：

| 关系 | 依赖 |
|------|------|
| 写后读 RAW | 本版本生产者 → 本版本读取者 |
| 写后写 WAW | 前版本生产者 → 后版本生产者 |
| 读后写 WAR | 前版本的全部读取者 → 后版本生产者 |

`resource(id).physical_resource` 指向导入根版本，`previous_version` 指向直接前驱；根版本的前驱为 `INVALID_RESOURCE_ID`。版本共享 GPU 句柄、格式、尺寸与 framebuffer。颜色附件也可以写入后继版本，隐式写入参与相同依赖规则。

版本链按物理覆盖顺序组织，分叉和无生产者的后继版本会使编译失败。一个 pass 对同一物理资源使用一个版本；跨版本的同 pass 采样/写入反馈、同时读取历史值需要独立物理资源。旧内容的读取发生在覆盖前，需要保留时先复制到其他资源。`reset()` 后重新导入并声明版本。

## 同步与导出

`barriers()[pass_id]` 是该 pass 的资源转换列表；`final_barriers()` 是导出转换列表。编译为首次使用、用途变化和写后访问生成 barrier，相同只读用途复用已有状态。状态按物理资源连续累计，版本切换保持之前的用途。barrier 的 `resource` 保留本次访问的逻辑版本编号。计划中的 `before == Undefined` 由驱动解析为实际源状态，外部输入和跨帧数据继续保留。

Vulkan 跟踪实际布局、最近写入以及其后的全部读取范围，保证写入可见性和后续覆盖顺序。颜色/深度附件保持 attachment optimal 布局，离屏纹理在下游读取前转换，交换链在 `end_frame()` 转换为 Present。同步模型见下文“当前边界与验证”。

`set_final_access(resource, access)` 对最后一个版本声明导出状态，在全部 pass 成功记录后用独立命令列表转换。旧版本设置导出状态会使编译失败，创建后继版本也不会继承前驱的导出要求。不指定时保留最后一次用途；指定 `Undefined` 可清除导出要求。以下是读回的生命周期：

```cpp
// readback pass 内声明 read(image, TransferRead)、write(buffer, TransferWrite)，
// 并调用 device.cmd_copy_texture_to_buffer(ctx.cmd, image_handle, buffer_handle)。
graph.set_final_access(buffer_id, ResourceAccess::HostRead);
bool recorded = graph.compile() && graph.execute(device);
device.end_frame();
device.present();
device.wait_idle();
if (recorded)
{
    const void* pixels = device.map_buffer(buffer_handle);
    // 检查 pixels != nullptr 后读取；GpuToCpu 映射会 invalidate 非一致缓存。
    device.unmap_buffer(buffer_handle);
}
```

`cmd_copy_texture_to_buffer` 支持单 mip、单层二维 `R8G8B8A8_UNORM/SRGB`，从缓冲偏移 0 写入紧密排列的数据，目标缓冲容量须足够。CPU 读回和资源释放在 GPU 完成后进行；同步上传在本帧命令录制前完成。驱动维护上传后的状态，调用方保证 CPU 写入时相关内存已结束 GPU 读取。

## 编译与错误

- 每一步选择注册编号最小的就绪 pass，保留全部已注册 pass。
- `passes()` 始终按注册顺序，pass ID 不随排序变化；`execution_order()` 是执行编号序列，`dependencies()[id]` 是该 pass 去重并排序后的前置 pass 编号。
- 非法资源编号（包括 `INVALID_RESOURCE_ID`）、无效导入句柄、重复物理资源导入、无效附件、同版本多个生产者、版本分叉或缺少生产者、同 pass 读写或使用多个物理同源版本、旧版本导出、冲突/不兼容用途、循环依赖均记录日志并使 `compile()` 返回 `false`。循环诊断列出被循环阻塞的 pass，可能包括其下游。
- 编译失败或图变化后，执行顺序、依赖与 barrier 列表为空，`execute()` 返回 `false` 且不记录命令。所有声明完成后重新调用 `compile()`。
- 无效 pass 编号的声明会使当前图无效，`reset()` 后可重新构建；有效 pass 的错误资源声明也可通过重新构建修正。

`resource(id)` 提供只读资源描述，图修改通过声明接口触发计划失效。`PassBuilder` 用于当前构建阶段，`reset()` 后须重新创建；执行回调期间修改图、编译或重入执行会被拒绝。

每个 pass 使用独立命令列表。打开命令列表、barrier 或最终导出失败时，`execute()` 返回 `false`，停止后续记录，保留已记录命令。回调内部的命令失败由回调或调用方处理。调用方管理 `begin_frame()`、`end_frame()`、`present()` 和导入资源的生命周期。

## 当前边界与验证

| 项目 | 当前契约 |
|------|----------|
| 同步范围 | 导入资源、整个资源粒度、跨 pass 访问 |
| 执行模型 | 单线程构建与执行，同一 graphics queue 串行记录 |
| 资源关系 | 物理资源导入一次，显式版本形成线性覆盖链，每个版本最多一个生产者 |
| 调用方同步 | pass 内部读写切换、原生 API 操作、持久映射写入和跨线程/队列访问由调用方管理；显存别名需要额外跟踪 |
| 后续工作 | 瞬态分配、异步队列、pass 剔除和子资源跟踪 |

`tests/services/frame_graph*_test.cpp` 使用 mock 验证拓扑顺序、错误图、用途验证、barrier 批次/位置、读回导出及失败边界。版本测试还覆盖三个连续版本、外部初始内容、全部 120 种五 pass 注册排列、跨版本 barrier、WAR 循环和非法版本声明。`tests/drivers/vulkan_sync_test.cpp` 在 CPU 上验证读写范围保留和布局映射。运行 `ctest --test-dir build --output-on-failure`。

真实 GPU 验证（需要显示与 Vulkan 设备）：

```bash
VK_LAYER_VALIDATE_SYNC=1 bin/debug/rover --demo frame-graph --frames 8
VK_LAYER_VALIDATE_SYNC=1 bin/debug/rover --demo triangle --frames 8
VK_LAYER_VALIDATE_SYNC=1 bin/debug/rover --demo mesh --scene main/builtin_assets/scenes/demos/primitives.rscene --frames 8
```

`frame-graph` 每帧将同一张 RGBA8 纹理写入两种颜色，并在每次覆盖前分别复制到两个回读缓冲。它先注册新版本消费者与覆盖 pass，再注册旧版本消费者与初始写入，编译得到初始写入 → 旧版本回读 → 覆盖 → 新版本回读。最终纹理导出 ShaderRead，两个缓冲导出 HostRead，等待 GPU 完成后分别验证每个版本的全部 4096 像素。两种颜色逐帧交换，覆盖跨帧状态复用。该入口采用逐帧等待。实际场景的 HDR 写入、shader 采样与回读链见 [离屏后处理](POST_PROCESS.md)，其中 pass 以消费者优先的顺序登记，由依赖编译得出执行顺序。异步读回性能另行验证。

2026-11-07 验证记录：Intel Iris Xe / Vulkan 1.3 上，双版本读回连续 32 帧通过；五个内置场景、窗口缩放及 triangle 回归各 8 帧通过，未出现 Vulkan 验证错误或 SYNC-HAZARD。

同步语义参考 Khronos [规范](https://docs.vulkan.org/spec/latest/chapters/synchronization.html) 与 [同步示例](https://docs.vulkan.org/guide/latest/synchronization_examples.html)。
