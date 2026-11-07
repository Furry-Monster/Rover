# 纹理子资源

纹理访问按 aspect、mip 和 array layer 跟踪。FrameGraph 为每个单元编译依赖与 barrier，Vulkan 保存对应的实际访问状态；上传、附件、采样和回读使用同一范围类型。设计见 [ADR-0018](../product/adr/ADR-0018-texture-subresources.md)。

## 范围与默认值

`TextureSubresourceRange` 定义在 `core/graphics/texture_subresource.h`。

| 字段 | 含义 |
|------|------|
| `aspects` | Color、Depth、Stencil 的组合；Automatic 选择格式的全部 aspect |
| `base_mip` / `mip_count` | 起始 mip 与数量；数量 0 表示直到末尾 |
| `base_layer` / `layer_count` | 起始层与数量；数量 0 表示直到末尾 |

默认 `{}` 表示整个纹理。`TEXTURE_BASE_SUBRESOURCE` 表示 mip 0、layer 0，aspect 根据格式确定。上传、复制和附件默认使用该单元；barrier、图的 read/write、导出、采样视图和清屏默认使用整个纹理。

`normalize_texture_range` 将剩余数量和 Automatic 展开为明确值，并验证格式与边界。Cube 的 `TextureDesc::array_layers` 表示 cube 数，范围中的 layer 表示物理面，共 `6 * array_layers` 层。3D 纹理的深度切片属于 mip 的体积，array layer 保持 1。

## 图声明与版本

导入多 mip/layer 纹理时使用 `import_texture(name, handle, TextureDesc, framebuffer)`，使编译器获得完整描述。原有 format/width/height 重载描述单 mip、单 layer 纹理。

```cpp
TextureSubresourceRange mip0{TextureAspect::Color, 0, 1, 0, 1};
TextureSubresourceRange mip1{TextureAspect::Color, 1, 1, 0, 1};
auto original = graph.import_texture("image.v0", image, desc);
auto updated = graph.create_version(original, "image.v1");
graph.add_pass("sample original mip0", [=](PassBuilder& b) {
    b.read(original, ResourceAccess::ShaderRead, mip0);
}, sample_commands);
graph.add_pass("overwrite mip1", [=](PassBuilder& b) {
    b.write(updated, ResourceAccess::TransferWrite, mip1);
}, clear_commands);
graph.set_final_access(original, ResourceAccess::ShaderRead, mip0);
```

每个逻辑版本的每个单元最多一个写入 pass。不同 pass 可以写入同版本的不重叠范围；同一 pass 可以用不同用途或版本访问不重叠单元。重叠单元要求版本、用途和读写方向一致，重复声明去重。

后继版本至少写入一个单元，其余单元继承前版本的内容与生产者。RAW/WAR/WAW 只对重叠单元建立依赖，覆盖前等待这些单元的旧读取者。瞬态纹理的未初始化单元不能被读取或导出为只读状态。

`set_final_access(id, access, range)` 允许导出未被后续版本覆盖的旧单元；同一物理单元的导出状态须一致。传入默认范围替换整个版本的导出要求；传入明确范围设置或替换该范围，Undefined 清除对应要求。创建版本不会继承导出声明。

资源池按完整物理纹理分配和租用，使用区间汇总所有子资源与版本。子资源独立同步不会把一张纹理拆成多个物理分配。

## 视图与附件

- `BindGroupEntry::texture_range` 选择单个 aspect 的采样或 storage 视图；范围可包含多个 mip/layer，须与 shader 的纹理类型匹配。
- `FramebufferDesc::color_ranges` 与颜色附件逐项对应，空列表使用基准单元；`depth_range` 选择深度附件单元。
- `set_render_target` 的 color_range/depth_range 选择图管理的附件。每个附件为单 mip、单 layer，framebuffer 尺寸等于对应 mip 的尺寸；颜色与深度尺寸一致。组合 depth/stencil 附件同时选择两个 aspect。
- 驱动缓存规范化范围的 VkImageView，随纹理释放。池的 framebuffer 与 bind group 缓存键包含范围，释放早于被引用的纹理。

## 上传、复制与清屏

`update_texture(texture, data, size, range)` 同步上传单 mip、单 aspect、连续 layer 的完整平面，紧密排列，size 必须精确匹配。Sampled 纹理上传后将所选单元转为 ShaderRead，其余纹理保持 TransferWrite。上传在本帧命令录制前完成。

`cmd_copy_texture_to_buffer(cmd, texture, buffer, TextureCopyRegion{range, offset})` 将同样的平面布局复制到缓冲。offset 按 4 字节对齐，容量覆盖所有所选层。纹理须处于 TransferRead，缓冲须处于 TransferWrite；CPU 映射前导出 HostRead 并等待 GPU 完成。

| 格式 / aspect | 每 texel 字节 |
|---------------|--------------|
| RGBA8 / BGRA8，UNORM 或 SRGB / Color | 4 |
| D32、D24S8、D32S8 / Depth | 4 |
| D24S8、D32S8 / Stencil | 1 |

组合 depth/stencil 分别上传、复制两个平面；D24 depth 使用 Vulkan 规定的 32 位平面表示。HDR 纹理仍支持渲染与采样，当前 CPU 上传/回读格式见上表。

`cmd_clear_texture(cmd, texture, clear, range)` 清屏整个所选范围，可包含多个 mip/layer 和 depth/stencil 两个 aspect。所选单元须处于 TransferWrite。

## Vulkan 同步与设备特性

驱动为每个单元保留最近访问、最近写入及后续读取的阶段/访问掩码。barrier 批次先完整验证，非法范围、重叠请求或未初始化读取均失败，且不改变跟踪状态。

设备创建时查询并启用 `separateDepthStencilLayouts`。支持时组合格式的 depth/stencil 可以独立转换；不支持时 barrier 同时覆盖两者，部分 aspect 的转换和同步上传返回失败。整资源转换继续有效，复制/清屏可在两者已共同转换后选择单个平面。

## 验证

`tests/services/frame_graph_subresource_test.cpp` 验证范围、独立生产者、局部版本继承、重叠依赖、导出、初始化和视图缓存。`tests/drivers/vulkan_sync_test.cpp` 验证独立状态和设备特性回退。

```bash
VK_LAYER_VALIDATE_SYNC=1 bin/debug/rover --demo subresources --frames 8
python3 misc/scripts/phase2_gpu_regression.py --exe bin/debug/rover --output-dir /tmp/rover-phase2-gpu
```

subresources demo 在 3 mip × 2 layer 纹理上验证局部上传、不同尺寸附件、采样视图、版本覆盖及未修改单元；再验证组合格式中独立 depth/stencil 覆盖与平面回读。完整验收结果见 [Phase 2 验收](PHASE2_ACCEPTANCE.md)。
