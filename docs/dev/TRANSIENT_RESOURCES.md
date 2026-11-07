# Frame Graph 瞬态资源

FrameGraph 声明本帧所需纹理、缓冲、绑定和 render target，TransientResourcePool 提供物理对象并按 GPU 提交完成状态回收。设计见 [ADR-0017](../product/adr/ADR-0017-frame-graph-transient-resources.md)。

## 调用顺序

1. 持久创建 `TransientResourcePool(device)`，其生命周期覆盖使用它的图执行。
2. `device.begin_frame()` 成功后，声明图资源和 pass。
3. `graph.compile()` 检查声明并计算物理根资源的使用区间。
4. `graph.prepare_resources(pool)` 创建或复用物理对象，准备 framebuffer 与 bind group。
5. 初始化 CpuToGpu 缓冲，然后 `graph.execute(device)`。
6. `device.end_frame()` / `present()`。回读在 GPU 完成后进行，下一次资源准备前消费结果。
7. 关闭时先清理池，再释放它引用的外部布局、采样器、render pass，最后关闭设备。

```cpp
TransientResourcePool pool(device); // 放在逐帧循环外。
FrameGraph graph;
auto camera = graph.create_buffer("camera", {sizeof(CameraUbo), BufferUsage::Uniform, MemoryUsage::CpuToGpu});
RenderBinding entry{};
entry.entry.type = BindingType::UniformBuffer;
entry.entry.buffer_size = sizeof(CameraUbo);
entry.resource = camera;
auto bindings = graph.create_bind_group(camera_layout, {entry});
graph.add_pass("scene", [&](PassBuilder& b) {
    b.read(camera, ResourceAccess::UniformRead);
}, [=](PassExecuteContext& ctx) {
    ctx.device.cmd_bind_pipeline(ctx.cmd, scene_pipeline);
    ctx.device.cmd_bind_group(ctx.cmd, 0, ctx.graph.bind_group(bindings));
    // 记录场景绘制。
});
if (graph.compile() && graph.prepare_resources(pool))
{
    device.update_buffer(graph.resource(camera).buffer, &camera_data, sizeof(camera_data));
    graph.execute(device);
}
```

颜色目标使用 `set_render_target(pass, render_pass, color, clear, depth)`。颜色与可选深度形成隐式写入；池在准备阶段创建与缓存 framebuffer。已有 import 和 `set_color_attachment` 继续支持调用方提供的 framebuffer。

## 使用区间与复用

`RenderResource::first_use/last_use` 是编译执行顺序中的位置，未使用为 `INVALID_PASS_ID`。同一资源的全部版本共享区间，导出把最后使用延长到图结束。未使用的瞬态声明不分配对象。GPU-only 资源的首次内容读取需要生产者；CpuToGpu 缓冲允许调用方在准备后初始化。

- 描述相同且区间不相交的 GPU-only 资源可复用同一物理对象，debug_name 不参与比较。
- 主机可见缓冲保留整帧租约，防止提交前的 CPU 更新覆盖前一 pass 的数据。
- 跨帧复用要求 `last_use_serial <= device.completed_serial()`，获取交换链图像不作为 CPU 写入安全证明。
- 版本与池内复用均保留 driver 的实际布局和访问状态；新逻辑资源的首个 barrier 由 driver 解析实际源状态。
- framebuffer 与 bind group 不可变，按具体句柄和描述缓存；调用方在 pass 中声明其资源用途。

## 失败与清理

资源准备在候选容器中完成，纹理、缓冲、framebuffer 或 bind group 创建失败后按依赖顺序撤销本次对象。成功后才更新图的句柄和池的租约。相同图在同一提交内重复准备直接复用当前结果。

`clear()` 等待 GPU 空闲，先释放绑定与 framebuffer，再释放资源，并推进池代数。已经准备的图因此失效，须重新准备。若当前帧已经记录了该池的命令但尚未提交，clear 返回 false，保留全部资源。池及引用它的图不支持并发修改。

## 验证

`tests/services/transient_resource_pool_test.cpp` 覆盖区间复用、在途提交隔离、版本区间、主机缓冲、初始化/用途验证、缓存、逐分配点回滚与未提交清理保护。后处理测试验证配置替换、读回与销毁顺序。

```bash
ctest --test-dir build --output-on-failure
python3 misc/scripts/phase2_gpu_regression.py --exe bin/debug/rover --output-dir /tmp/rover-phase2-gpu
```

2026-11-07：Intel Iris Xe / Vulkan 1.3，32 帧双版本及后续区间复用共用 1 张纹理、3 个回读缓冲；120 帧无逐帧等待的 mesh 渲染使用 6 张中间纹理与 2 个相机缓冲。五个场景、缩放和 triangle 回归均通过，无 Vulkan 验证或同步错误。
