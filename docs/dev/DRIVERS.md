# Rover 引擎 Drivers 子系统文档

> **权威文档** — 对 `drivers/` 的任何修改都 **必须** 同步更新本文档。修改前请先阅读本文档了解现有设计。

---

## 目录

1. [概述](#1-概述)
2. [文件清单](#2-文件清单)
3. [基础设施 — `vk_common.h` / `vk_format.{h,cpp}`](#3-基础设施--vk_commonh--vk_formathcpp)
4. [VkInstanceWrapper — 实例与验证层](#4-vkinstancewrapper--实例与验证层)
5. [VkDeviceWrapper — 物理设备与逻辑设备](#5-vkdevicewrapper--物理设备与逻辑设备)
6. [VkSwapchainWrapper — 交换链](#6-vkswapchainwrapper--交换链)
7. [VkResourcePool — 句柄到 Vulkan 对象的映射](#7-vkresourcepool--句柄到-vulkan-对象的映射)
8. [GraphicsDeviceVulkan — 顶层类](#8-graphicsdevicevulkan--顶层类)
9. [生命周期](#9-生命周期)
10. [资源管理](#10-资源管理)
11. [命令录制](#11-命令录制)
12. [帧管理与同步](#12-帧管理与同步)
13. [VMA 集成](#13-vma-集成)
14. [验证层启用](#14-验证层启用)
15. [构建集成](#15-构建集成)
16. [扩展到其他后端](#16-扩展到其他后端)
17. [后续工作](#17-后续工作)

---

## 1. 概述

`drivers/` 是 Rover 引擎的 **Layer 2**，实现 `core/graphics/graphics_device.h` 中声明的纯虚 `GraphicsDevice` 接口。当前唯一已实现的后端是 **Vulkan 1.3**，位于 `drivers/vulkan/`。

设计原则（依赖倒置）：
- `core/graphics/` 声明 `GraphicsDevice` 抽象接口与所有 GPU 资源类型（`BufferHandle`、`TextureDesc` 等）
- `drivers/vulkan/` 提供 `GraphicsDeviceVulkan` 实现
- `services/` 与 `main/` 仅依赖 `GraphicsDevice` 抽象，不知道具体后端
- 通过 `register_vulkan_driver()` / `get_vulkan_device()` 函数获取实例的 `GraphicsDevice*`（不是 `GraphicsDeviceVulkan*`，避免 Vulkan + X11 头文件污染上层）

| 子系统 | 文件数 | 职责 |
|--------|--------|------|
| 基础设施 | 5 | 错误检查、格式转换、句柄常量 |
| 实例 / 设备 / 交换链 | 6 | Vulkan 启动序列 |
| 资源池 | 1 | 句柄 ↔ VkXxx 映射 |
| 设备实现 | 8 | `GraphicsDeviceVulkan` 类与所有方法 |

CMake target：`rover_driver_vulkan`（具体）+ `rover_drivers`（聚合，别名 `Rover::Drivers`）。`rover_driver_vulkan` PUBLIC 暴露宏 `ROVER_DRIVER_VULKAN=1`。

依赖：`Rover::Core` + `Rover::Vulkan`（包含 `volk`、`VulkanMemoryAllocator`、`Vulkan::Headers`）。

---

## 2. 文件清单

```
drivers/
├── register_driver_types.{h,cpp}      条件分发到具体驱动
├── CMakeLists.txt                      聚合 façade
│
└── vulkan/
    ├── vk_common.h                     VK_CHECK 宏，常量
    ├── vk_format.{h,cpp}               rover ↔ Vulkan 类型转换
    ├── vk_instance.{h,cpp}             VkInstance + 调试消息回调
    ├── vk_device.{h,cpp}               物理设备选择 + 逻辑设备 + 队列
    ├── vk_swapchain.{h,cpp}            交换链 + 图像视图 + 每图像信号量
    ├── vk_resource_pool.h              通用句柄→资源映射模板
    │
    ├── graphics_device_vulkan.h        类声明 + 资源结构体定义
    ├── graphics_device_vulkan_lifecycle.cpp     init/shutdown/recreate_swapchain
    ├── graphics_device_vulkan_buffer.cpp        buffer/texture/sampler
    ├── graphics_device_vulkan_shader.cpp        shader/render_pass/framebuffer
    ├── graphics_device_vulkan_pipeline.cpp      graphics pipeline
    ├── graphics_device_vulkan_commands.cpp      命令录制
    ├── graphics_device_vulkan_frame.cpp         begin/end_frame, present
    │
    ├── register_types.{h,cpp}          register_vulkan_driver / get_vulkan_device
    └── CMakeLists.txt                  rover_driver_vulkan 静态库
```

总计 **19 个文件**（11 个 `.h` + 8 个 `.cpp`），约 **2300 行**实现代码。

---

## 3. 基础设施 — `vk_common.h` / `vk_format.{h,cpp}`

### 3.1 `vk_common.h`

错误检查宏与全局常量：

```cpp
#define VK_CHECK(expr)             // 记录错误日志后继续执行
#define VK_CHECK_RETURN(expr, ret) // 记录错误后立即 return 指定值

inline constexpr u32 kMaxFramesInFlight = 2;
```

`VK_CHECK*` 宏将 `VkResult` 错误码、源文件、行号通过 `ROVER_LOG_ERROR` 输出。所有非平凡 Vulkan 调用都应包裹这些宏。

### 3.2 `vk_format.{h,cpp}` — 类型转换

将 `core/graphics/graphics_types.h` 中的 rover 枚举映射到 Vulkan 等价物：

| rover 类型 | Vulkan 类型 | 用途 |
|-----------|-------------|------|
| `Format` | `VkFormat` | 像素 / 顶点格式 |
| `PrimitiveTopology` | `VkPrimitiveTopology` | 图元类型 |
| `CullMode` | `VkCullModeFlags` | 剔除模式 |
| `FrontFace` | `VkFrontFace` | 正面方向 |
| `CompareOp` | `VkCompareOp` | 比较操作（深度、模板） |
| `BlendFactor` | `VkBlendFactor` | 混合因子 |
| `BlendOp` | `VkBlendOp` | 混合方程 |
| `LoadOp` | `VkAttachmentLoadOp` | 附件加载操作 |
| `StoreOp` | `VkAttachmentStoreOp` | 附件存储操作 |
| `Filter` | `VkFilter` | 采样过滤 |
| `SamplerAddressMode` | `VkSamplerAddressMode` | 采样寻址 |
| `IndexType` | `VkIndexType` | 索引类型 |
| `BufferUsage` | `VkBufferUsageFlags` | 缓冲用途位 |
| `TextureUsage` | `VkImageUsageFlags` | 图像用途位 |
| `ShaderStage` | `VkShaderStageFlagBits` | 着色器阶段 |
| `TextureType` | `VkImageType` / `VkImageViewType` | 图像维度 |

`from_vk_format()` 提供反向映射，主要用于 `get_swapchain_format()`。

---

## 4. VkInstanceWrapper — 实例与验证层

### 4.1 类签名

```cpp
class VkInstanceWrapper {
public:
    bool init(const std::vector<const char*>& required_extensions, bool enable_validation);
    void shutdown();

    [[nodiscard]] VkInstance handle() const;
    [[nodiscard]] bool       validation_enabled() const;

private:
    VkInstance               instance_        = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
};
```

### 4.2 init 流程

1. `volkInitialize()` — 加载 Vulkan loader（`libvulkan.so` / `vulkan-1.dll`）
2. 复制 `required_extensions`，如启用验证则追加 `VK_EXT_DEBUG_UTILS_EXTENSION_NAME`
3. 检查 `VK_LAYER_KHRONOS_validation` 是否可用：
   - 若不可用，记录警告并禁用验证（不致命）
   - 若可用且启用，加入 `enabled_layer_count`
4. `vkCreateInstance` 创建 instance（API 1.3，applicationInfo 标识为 "RoverEngine"）
5. `volkLoadInstance(instance_)` — 通过 volk 加载所有 instance-level 函数指针
6. 若启用验证：创建 `VkDebugUtilsMessengerEXT`，回调将 Vulkan 消息转发到 `ROVER_LOG_*`：
   - `VERBOSE` / `INFO` → `ROVER_LOG_TRACE`
   - `WARNING` → `ROVER_LOG_WARN`
   - `ERROR` → `ROVER_LOG_ERROR`

### 4.3 shutdown 流程

按反向顺序销毁：先 debug messenger，再 instance。

---

## 5. VkDeviceWrapper — 物理设备与逻辑设备

### 5.1 类签名

```cpp
struct QueueFamilyIndices {
    u32 graphics = ~0u;
    u32 present  = ~0u;
    [[nodiscard]] bool is_complete() const;
};

class VkDeviceWrapper {
public:
    bool init(VkInstance instance, VkSurfaceKHR surface);
    void shutdown();

    [[nodiscard]] VkPhysicalDevice physical()      const;
    [[nodiscard]] VkDevice         logical()       const;
    [[nodiscard]] VkQueue          graphics_queue() const;
    [[nodiscard]] VkQueue          present_queue() const;
    [[nodiscard]] const QueueFamilyIndices& queue_families() const;
    [[nodiscard]] const char*               device_name() const;
    [[nodiscard]] const VkPhysicalDeviceProperties& properties() const;
};
```

### 5.2 物理设备选择算法

`score_device()` 给每个候选设备打分，选最高分：

| 条件 | 分数 |
|------|------|
| 不支持必需扩展（`VK_KHR_swapchain`） | -1（跳过） |
| Surface 无可用格式或 present mode | -1 |
| 不满足队列族要求（graphics + present） | -1 |
| 满足基础要求 | 100 |
| 离散 GPU | +1000 |
| 集成 GPU | +500 |

### 5.3 逻辑设备创建

- 使用 `std::set<u32>` 去重队列族（graphics 和 present 可能是同一族）
- 启用 `samplerAnisotropy` 特性
- 启用扩展：`VK_KHR_SWAPCHAIN_EXTENSION_NAME`
- `volkLoadDevice(device_)` 加载 device-level 函数指针（性能更优于 instance-level dispatch）

---

## 6. VkSwapchainWrapper — 交换链

### 6.1 类签名

```cpp
class VkSwapchainWrapper {
public:
    bool init(VkInstance, VkPhysicalDevice, VkDevice, VkSurfaceKHR,
              const QueueFamilyIndices&, u32 width, u32 height);
    void shutdown(VkDevice);
    bool recreate(VkPhysicalDevice, VkDevice, VkSurfaceKHR,
                  const QueueFamilyIndices&, u32 width, u32 height);

    [[nodiscard]] VkSwapchainKHR             handle()       const;
    [[nodiscard]] VkFormat                   image_format() const;
    [[nodiscard]] VkExtent2D                 extent()       const;
    [[nodiscard]] u32                        image_count()  const;
    [[nodiscard]] const std::vector<VkImage>&     images()      const;
    [[nodiscard]] const std::vector<VkImageView>& image_views() const;

    [[nodiscard]] VkSemaphore render_finished_semaphore(u32 image_index) const;

    bool acquire_next_image(VkDevice, VkSemaphore signal_sem, u32* image_index_out);
    bool present(VkQueue, VkSemaphore wait_sem, u32 image_index);
};
```

### 6.2 创建参数选择

- **Surface 格式**：优先 `VK_FORMAT_B8G8R8A8_SRGB` + `SRGB_NONLINEAR`，否则取第一个
- **Present mode**：优先 `VK_PRESENT_MODE_MAILBOX_KHR`（低延迟，无撕裂），否则 `VK_PRESENT_MODE_FIFO_KHR`（即垂直同步，规范保证可用）
- **Extent**：若 `caps.currentExtent.width == UINT32_MAX` 则使用窗口 framebuffer 尺寸（高 DPI 适配），否则使用 currentExtent
- **图像数**：`caps.minImageCount + 1`，clamp 到 `maxImageCount`（一般为 3-4）
- **共享模式**：graphics 与 present 队列族不同时使用 `CONCURRENT`，相同时使用 `EXCLUSIVE`

### 6.3 每图像信号量（render_finished）

**重要**：`render_finished_semaphore` 必须 **每个交换链图像一个**，而不是每帧（`kMaxFramesInFlight=2`）一个。原因：

- `vkQueuePresentKHR` 等待这个信号量
- 信号量必须在 present 完成（图像可重新获取）后才能再次被等待
- 仅 in-flight fence 不足以保证 present 已完成（fence 仅追踪 GPU 提交，不追踪 present 队列）
- 解决方案：信号量与图像绑定，4 个图像 = 4 个信号量。每个图像本周期被使用前，上次的 present 必然已完成（否则不会再次被 acquire）

参考：<https://github.com/KhronosGroup/Vulkan-Docs/issues/152>

### 6.4 acquire / present 错误处理

- `VK_ERROR_OUT_OF_DATE_KHR` → 返回 false，调用方应触发 `recreate_swapchain`
- `VK_SUBOPTIMAL_KHR` → 视为成功（仍可渲染当前帧），但应推迟 recreate

---

## 7. VkResourcePool — 句柄到 Vulkan 对象的映射

### 7.1 模板类

```cpp
template <typename T>
class VkResourcePool {
public:
    u64 add(T resource);                          // 返回新句柄（从 1 开始自增，0 = INVALID_HANDLE）
    T*  get(u64 handle);                          // 返回 nullptr 表示无效
    bool remove(u64 handle, T* out_removed = nullptr);
    template <typename Fn> void for_each(Fn&& fn);
    void clear();
    [[nodiscard]] usize size() const;

private:
    std::unordered_map<u64, T> resources_;
    u64                        next_handle_ = 1;
};
```

### 7.2 实例化

`GraphicsDeviceVulkan` 中实例化以下池：

| 池 | 元素类型 | 存储内容 |
|----|---------|---------|
| `buffers_` | `VkBufferResource` | VkBuffer + VmaAllocation + 大小/用途/映射状态 |
| `textures_` | `VkTextureResource` | VkImage + VkImageView + VmaAllocation + 元数据 |
| `samplers_` | `VkSampler` | 直接存 VkSampler 句柄 |
| `shaders_` | `VkShaderResource` | VkShaderModule + 阶段 |
| `render_passes_` | `VkRenderPassResource` | VkRenderPass + 颜色格式列表 + 是否有深度 |
| `framebuffers_` | `VkFramebufferResource` | VkFramebuffer + 尺寸 + 关联的 RenderPass |
| `pipelines_` | `VkPipelineResource` | VkPipeline + VkPipelineLayout |
| `command_lists_` | `VkCommandListResource` | VkCommandBuffer + recording 标志 + submitted 标志 |

### 7.3 句柄稳定性

- 句柄在创建时分配，永不重用（即使资源被 destroy）
- `remove()` 后再次 `get()` 同一句柄会返回 `nullptr`
- 0 永远表示 `INVALID_HANDLE`，`add()` / `get()` / `remove()` 都对 0 安全

---

## 8. GraphicsDeviceVulkan — 顶层类

### 8.1 资源结构体（位于 `graphics_device_vulkan.h`）

```cpp
struct VkBufferResource {
    VkBuffer       buffer;
    VmaAllocation  allocation;
    VkDeviceSize   size;
    bool           mapped;
    void*          mapped_ptr;
    BufferUsage    usage;
    MemoryUsage    memory;
};

struct VkTextureResource {
    VkImage        image;
    VkImageView    view;
    VmaAllocation  allocation;
    VkFormat       format;
    VkExtent3D     extent;
    u32            mip_levels;
    u32            array_layers;
    bool           owns_image;     // 交换链图像为 false，避免 destroy_*_swapchain 双重释放
};

struct VkShaderResource       { VkShaderModule module; VkShaderStageFlagBits stage; };
struct VkRenderPassResource   { VkRenderPass pass; std::vector<Format> color_formats; bool has_depth; };
struct VkFramebufferResource  { VkFramebuffer framebuffer; u32 width, height, attachment_count; RenderPassHandle render_pass; };
struct VkPipelineResource     { VkPipeline pipeline; VkPipelineLayout layout; };
struct VkCommandListResource  { VkCommandBuffer buffer; bool recording; bool submitted; };

struct FrameSync {              // 每个 frame-in-flight 一份
    VkSemaphore image_available;
    VkFence     in_flight;
    // render_finished 由 swapchain 拥有（每图像一份）
};
```

### 8.2 主类成员

```cpp
class GraphicsDeviceVulkan : public GraphicsDevice {
private:
    WindowSystem*          window_;          // 由 init(window) 设置
    VkSurfaceKHR           surface_;
    VkInstanceWrapper      instance_;
    VkDeviceWrapper        device_;
    VmaAllocator           vma_allocator_;
    VkSwapchainWrapper     swapchain_;

    VkCommandPool          command_pool_;

    std::array<FrameSync, kMaxFramesInFlight>                       frame_sync_;
    std::array<std::vector<CommandListHandle>, kMaxFramesInFlight>  retired_command_lists_;
    u32                    current_frame_;     // [0, kMaxFramesInFlight)
    u32                    current_image_;     // swapchain image index this frame

    /* 8 个 VkResourcePool<...> */

    std::vector<TextureHandle>     swapchain_texture_handles_;  // 交换链图像注册为只读纹理
    std::vector<CommandListHandle> frame_command_lists_;        // 当前帧已 begin 的命令列表

    bool in_frame_;
    bool swapchain_dirty_;
    char api_name_[32]      = "Vulkan 1.3";
};
```

---

## 9. 生命周期

### 9.1 `init(WindowSystem& window)`

实现于 `graphics_device_vulkan_lifecycle.cpp`，顺序：

1. **保存 window 引用**：`window_ = &window`
2. **查询所需 Vulkan 扩展**：`window.get_vulkan_required_extensions(required_extensions)`
3. **决定是否启用验证**：`#ifdef ROVER_DEBUG` 启用，Release 关闭
4. **创建 instance**：`instance_.init(required_extensions, enable_validation)`
5. **创建 surface**：`window.create_vulkan_surface(instance_.handle(), &surface_raw)`，再 `reinterpret_cast` 为 `VkSurfaceKHR`
6. **创建逻辑设备**：`device_.init(instance, surface)`
7. **创建 VMA allocator**：填 `VmaVulkanFunctions`、设置 API 1.3、调用 `vmaCreateAllocator`
8. **创建 swapchain**：`swapchain_.init(...)` + `register_swapchain_textures()` 把 swapchain 图像注册为 `VkTextureResource`（owns_image=false），写入 `swapchain_texture_handles_`
9. **创建 command pool**：`VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` flag，graphics 队列族
10. **创建每帧 sync**：每个 `FrameSync` 创建 `image_available` 信号量与 `in_flight` 围栏（fence 用 `SIGNALED_BIT` 创建，避免首帧死锁）

### 9.2 `shutdown()`

反向顺序销毁：

1. `vkDeviceWaitIdle` — 等待 GPU 完成所有工作
2. 遍历每个资源池，调用对应的 vkDestroy*
3. **特殊处理 textures_**：仅销毁 `owns_image == true` 的纹理，避免双重释放交换链图像/视图
4. 清理 `frame_command_lists_`、`retired_command_lists_[]`
5. 销毁每帧信号量与围栏
6. 销毁 command pool
7. `swapchain_.shutdown()` — 销毁 swapchain image views、render_finished 信号量、swapchain 本身
8. `vmaDestroyAllocator`
9. `vkDestroySurfaceKHR`
10. `device_.shutdown()`
11. `instance_.shutdown()`
12. 重置所有状态变量

### 9.3 `recreate_swapchain()`

窗口缩放或 swapchain 过期时调用：

1. `vkDeviceWaitIdle`
2. `unregister_swapchain_textures()` — 从 `textures_` 池移除 swapchain 句柄（不销毁底层资源，因为 swapchain 还会处理）
3. `swapchain_.recreate(...)` — 内部 `destroy_internal` + `create_internal`
4. `register_swapchain_textures()` — 重新注册新 swapchain 图像
5. `swapchain_dirty_ = false`

**注意**：framebuffer 池中的 framebuffer 引用旧 swapchain 图像句柄，外部主循环必须负责重建 framebuffer（`main.cpp` 的 `rebuild_framebuffers()`）。

---

## 10. 资源管理

### 10.1 Buffer

- `create_buffer`：用 VMA 分配，自动追加 `TRANSFER_DST_BIT | TRANSFER_SRC_BIT`，CPU 可访问的缓冲使用 `HOST_ACCESS_SEQUENTIAL_WRITE_BIT`
- `destroy_buffer`：若映射先 unmap，再 `vmaDestroyBuffer`
- `map_buffer` / `unmap_buffer`：缓存映射指针，避免重复 map
- `update_buffer(handle, data, size, offset=0)`：内部 map + memcpy + unmap（适用于 host-visible 缓冲；GpuOnly 缓冲需要 staging buffer，当前未实现）

### 10.2 Texture

- `create_texture`：VMA 分配 image + 创建 ImageView。同时追加 `VK_IMAGE_USAGE_TRANSFER_DST_BIT` 用于上传
- `destroy_texture`：仅销毁 `owns_image == true` 的资源；交换链图像静默忽略（防双重释放）
- `update_texture`：当前 stub（输出 WARN）。完整实现需要 staging buffer + layout transition + `vkCmdCopyBufferToImage`，留待后续

### 10.3 Sampler

- 直接存 `VkSampler` 句柄，无额外元数据
- `create_sampler`：使用描述符配置 mag/min filter、address mode、各向异性

### 10.4 Shader

- `create_shader`：校验 `bytecode_size % 4 == 0`，调用 `vkCreateShaderModule`
- `bytecode` 应为 SPIR-V 字节流
- `entry_point` 字段在创建时不使用（pipeline 创建时使用，固定为 `"main"`）

### 10.5 RenderPass

- 单一 subpass（v1 简化）
- 颜色附件 `finalLayout` 固定为 `PRESENT_SRC_KHR`（适合 swapchain 输出）
- 若 `has_depth_stencil`，深度附件 `finalLayout` 为 `DEPTH_STENCIL_ATTACHMENT_OPTIMAL`
- 单个 subpass dependency 同步颜色 + 深度 access（`SUBPASS_EXTERNAL` → 0）

### 10.6 Framebuffer

- 查找 `render_passes_` 池获取关联的 RenderPass
- 查找 `textures_` 池获取每个附件的 ImageView
- 校验所有句柄有效性
- 创建 `VkFramebuffer`，单层（`layers = 1`）

### 10.7 Pipeline

`graphics_device_vulkan_pipeline.cpp` 创建 graphics pipeline：

| 状态 | 来源 |
|------|------|
| Shader stages | `desc.vertex_shader` + `desc.fragment_shader`，入口 `"main"` |
| Vertex input | `desc.vertex_bindings`（每个 binding 含 attributes） |
| Input assembly | `desc.topology` |
| Viewport / Scissor | **动态状态**（运行时 `cmd_set_viewport` / `cmd_set_scissor` 设置） |
| Rasterization | `cull_mode`、`front_face`，固定 `polygonMode = FILL`，`lineWidth = 1.0` |
| Multisample | 固定 1 sample（无 MSAA） |
| Depth/stencil | 仅当 render pass 包含深度附件时启用 |
| Color blend | 每附件按 `desc.blend_*` 配置；颜色写入掩码 `RGBA` |
| Pipeline layout | 空（无 descriptor set / push constant，v1 限制） |
| Render pass + subpass | 来自 `desc.render_pass`，subpass = 0 |

`destroy_pipeline` 同时释放 `VkPipeline` 与 `VkPipelineLayout`。

---

## 11. 命令录制

### 11.1 begin / end

- `begin_command_list`：从 `command_pool_` 分配新 `VkCommandBuffer`，`vkBeginCommandBuffer` 用 `ONE_TIME_SUBMIT_BIT`，加入 `frame_command_lists_`，返回新句柄
- `end_command_list`：调用 `vkEndCommandBuffer`，标记 `recording = false`

### 11.2 命令操作

| 方法 | 对应 vk 命令 |
|------|-------------|
| `cmd_begin_render_pass` | `vkCmdBeginRenderPass`（INLINE 内容） |
| `cmd_end_render_pass` | `vkCmdEndRenderPass` |
| `cmd_bind_pipeline` | `vkCmdBindPipeline`（GRAPHICS bind point） |
| `cmd_set_viewport` | `vkCmdSetViewport`（动态状态） |
| `cmd_set_scissor` | `vkCmdSetScissor` |
| `cmd_bind_vertex_buffer` | `vkCmdBindVertexBuffers` |
| `cmd_bind_index_buffer` | `vkCmdBindIndexBuffer` |
| `cmd_draw` | `vkCmdDraw` |
| `cmd_draw_indexed` | `vkCmdDrawIndexed` |

### 11.3 ClearValue 处理

`cmd_begin_render_pass` 接收 `const ClearValue*`、count，转换为 `VkClearValue` 数组：
- 第 N 个 ClearValue 是颜色还是深度，取决于 render pass 是否最后一个附件是深度
- 颜色：填 `clearValue.color.float32[0..3]`
- 深度：填 `clearValue.depthStencil.depth + stencil`

---

## 12. 帧管理与同步

### 12.1 同步对象生命周期

| 对象 | 数量 | 拥有者 | 生命周期 |
|------|------|--------|---------|
| `image_available` 信号量 | `kMaxFramesInFlight` (=2) | `frame_sync_` | 每帧轮替 |
| `in_flight` 围栏 | `kMaxFramesInFlight` (=2) | `frame_sync_` | 每帧轮替 |
| `render_finished` 信号量 | swapchain image 数 (3-4) | `swapchain_` | 与图像绑定 |

### 12.2 begin_frame

```
1. 等待 frame_sync_[current_frame_].in_flight 围栏
   → 此时 GPU 已完成本 frame slot 上次的提交
2. 从 retired_command_lists_[current_frame_] 释放上次的 command buffers
3. swapchain_.acquire_next_image(image_available_sem, &current_image_)
   → 失败时设置 swapchain_dirty_，调用 recreate_swapchain，返回 false
4. 重置 in_flight 围栏（已被 vkWaitForFences 消费）
5. in_frame_ = true; 返回 true
```

### 12.3 end_frame

```
1. 收集 frame_command_lists_ 中所有 VkCommandBuffer
2. 对未结束录制的发出警告并 vkEndCommandBuffer
3. 取本图像的 render_finished 信号量
4. vkQueueSubmit:
   - waitSemaphore: image_available
   - waitDstStageMask: COLOR_ATTACHMENT_OUTPUT_BIT
   - commandBuffers: 全部
   - signalSemaphore: render_finished (per-image)
   - fence: in_flight
5. in_frame_ = false
```

### 12.4 present

```
1. 取本图像的 render_finished 信号量
2. swapchain_.present(present_queue, render_finished_sem, current_image_)
3. 失败 / OUT_OF_DATE → swapchain_dirty_ = true
4. 把 frame_command_lists_ 移到 retired_command_lists_[current_frame_]
   → 这些 buffer 将在下次本 frame slot begin_frame 时被释放
5. current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight
6. 若 swapchain_dirty_，立即 recreate_swapchain
```

### 12.5 命令缓冲生命周期管理

**关键不变量**：`vkFreeCommandBuffers` 不能在 GPU 仍在执行命令时调用。

正确的回收策略：
- 提交命令缓冲时，将其句柄存入 `retired_command_lists_[current_frame_]`
- 下次相同 frame slot 进入 `begin_frame` 时，`in_flight` 围栏已经信号 → GPU 必然完成
- 此时安全地 `vkFreeCommandBuffers` 并从 `command_lists_` 池移除

错误模式（已在初版修复）：在 `present()` 中立即 free 命令缓冲会触发验证错误 `pCommandBuffers[0] is in use`。

---

## 13. VMA 集成

### 13.1 编译宏（`Rover::Vulkan` 提供）

```
VMA_STATIC_VULKAN_FUNCTIONS=0
VMA_DYNAMIC_VULKAN_FUNCTIONS=1   ← 与 volk 配合
```

### 13.2 实现展开

VMA 是 header-only 库，但需要在 **恰好一个** translation unit 中定义 `VMA_IMPLEMENTATION` 实例化函数体：

```cpp
// drivers/vulkan/graphics_device_vulkan_lifecycle.cpp 中：
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
```

其他 `.cpp` 文件直接 `#include <vk_mem_alloc.h>` 使用即可。

### 13.3 创建 allocator

```cpp
VmaVulkanFunctions vma_funcs{};
vma_funcs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
vma_funcs.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;
// 其余字段由 VMA 内部 fetch（依赖上面两个）

VmaAllocatorCreateInfo info{};
info.physicalDevice   = device_.physical();
info.device           = device_.logical();
info.instance         = instance_.handle();
info.vulkanApiVersion = VK_API_VERSION_1_3;
info.pVulkanFunctions = &vma_funcs;
vmaCreateAllocator(&info, &vma_allocator_);
```

### 13.4 内存用途映射

| `MemoryUsage` | `VmaMemoryUsage` | 用途 |
|--------------|------------------|------|
| `GpuOnly` | `VMA_MEMORY_USAGE_GPU_ONLY` | GPU 私有，CPU 不可见（顶点/索引缓冲、纹理） |
| `CpuToGpu` | `VMA_MEMORY_USAGE_CPU_TO_GPU` | Staging buffer、uniform buffer |
| `GpuToCpu` | `VMA_MEMORY_USAGE_GPU_TO_CPU` | Readback、screenshot |

CPU 可访问的缓冲自动加上 `VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT`。

---

## 14. 验证层启用

### 14.1 编译期开关

`init(window)` 中：

```cpp
#ifdef ROVER_DEBUG
constexpr bool enable_validation = true;
#else
constexpr bool enable_validation = false;
#endif
```

### 14.2 运行时启用

如果系统未安装 `VK_LAYER_KHRONOS_validation`，driver 会记录警告并 **降级为无验证模式**（不致命）。

### 14.3 通过环境变量启用

如果 Vulkan SDK 安装在非标准路径，可通过环境变量让 loader 找到：

```bash
VK_LAYER_PATH=/path/to/vulkan-sdk/share/vulkan/explicit_layer.d \
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation \
./bin/debug/rover
```

或安装到系统路径：`apt install vulkan-validationlayers`（Debian/Ubuntu）。

### 14.4 调试消息回调

回调过滤掉 INFO/VERBOSE 噪音，将 WARNING/ERROR 输出到引擎日志。回调上下文里 `pCallbackData->pMessage` 是已格式化的字符串。

---

## 15. 构建集成

### 15.1 vendor 提供

`vendor/CMakeLists.txt` 中 `Rover::Vulkan` interface target 链接：
- `volk::volk` — 动态加载与 dispatch
- `VulkanMemoryAllocator` — VMA header-only 库
- `Vulkan::Headers` — Vulkan SDK 头文件（来自 `find_package(Vulkan)`）

平台相关 volk 编译宏（如 Linux 上的 `VK_USE_PLATFORM_XLIB_KHR;VK_USE_PLATFORM_WAYLAND_KHR`）在 vendor/CMakeLists.txt 中按 OS 配置。

### 15.2 着色器编译

由 `misc/cmake/RoverShader.cmake` 提供，使用 `glslangValidator` 编译 GLSL → SPIR-V → C 头文件：

```cmake
include(RoverShader)
rover_add_shader(_shader_headers
    SHADER     "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert.glsl"
    STAGE      vert
    VAR        triangle_vert_spv
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
```

输出形如 `static const uint32_t triangle_vert_spv[] = { ... };` 的头文件，可直接 `#include` 用作 `ShaderDesc::bytecode`。

### 15.3 main 中的注册

`main.cpp` 通过抽象接口使用 driver：

```cpp
#include "drivers/vulkan/register_types.h"
#include "core/graphics/graphics.h"
// 不要 include "drivers/vulkan/graphics_device_vulkan.h"
//   原因：透传 vulkan.h → X11 → #define None 0 → 与 CullMode::None 冲突

GraphicsDevice* device = get_vulkan_device();   // 抽象基类指针
device->init(window);                           // window 实现 WindowSystem 接口
```

---

## 16. 扩展到其他后端

新增 `<api>` 后端（如 D3D12、Metal）的步骤：

1. 创建 `drivers/<api>/` 目录
2. 实现 `class GraphicsDevice<API> : public GraphicsDevice` 覆盖所有纯虚方法
3. 编写 `register_types.{h,cpp}` 提供 `register_<api>_driver()` / `unregister_<api>_driver()` / `get_<api>_device()`
4. 编写 `CMakeLists.txt`：`target_compile_definitions(rover_driver_<api> PUBLIC ROVER_DRIVER_<API>=1)`
5. 在 `drivers/CMakeLists.txt` 添加条件分支启用编译
6. 在 `drivers/register_driver_types.cpp` 添加 `#ifdef ROVER_DRIVER_<API>` 调用入口

平台兼容性由 CMake 选项控制（`ROVER_D3D12` 仅 Windows，`ROVER_METAL` 仅 macOS/iOS）。

`WindowSystem` 接口已设计为 backend-agnostic：D3D12 后端可以扩展该接口，添加 `create_d3d12_swap_chain(...)` 类方法（保持 `void*` 不透明）。

---

## 17. 后续工作

### 已完成

- [x] Vulkan 1.3 实例 + 验证层 + 调试消息
- [x] 物理设备评分选择 + 逻辑设备 + 队列族
- [x] VMA 内存分配器集成
- [x] Swapchain（4 图像，FIFO/MAILBOX，sRGB）
- [x] 每图像 render_finished 信号量（修复多帧 in-flight 同步）
- [x] 命令缓冲池 + 安全回收（基于 in-flight fence）
- [x] Buffer / Texture / Sampler / Shader 创建销毁
- [x] RenderPass / Framebuffer 创建销毁
- [x] Graphics Pipeline 创建销毁（动态 viewport + scissor）
- [x] 命令录制（render pass、pipeline bind、draw、draw_indexed）
- [x] begin/end_frame + present + 双帧 in-flight
- [x] Swapchain 重建（窗口缩放、out-of-date）
- [x] 在屏幕上渲染彩色三角形（Phase 1 里程碑）

### 待做

- [ ] `update_texture`：staging buffer + image layout transition + `vkCmdCopyBufferToImage`
- [ ] Staging buffer 上传（`update_buffer` 对 GpuOnly 缓冲）
- [ ] Descriptor set / bind group 抽象（uniform buffer、纹理采样）
- [ ] Push constant 支持
- [ ] Pipeline cache（加速重建）
- [ ] Compute pipeline + dispatch
- [ ] 同步原语（semaphore / fence / event）暴露给上层（用于异步资源传输）
- [ ] Multi-sampled rendering（MSAA attachment）
- [ ] Mipmap 生成
- [ ] Cubemap / 3D 纹理上传
- [ ] Indirect draw
- [ ] Query pool（timestamp / occlusion query）
- [ ] Debug 标签（`vkCmdBeginDebugUtilsLabelEXT` 配合 RenderDoc）
- [ ] 多驱动协调（Windows 上 Vulkan + D3D12 共存）

---

*Rover Engine Drivers v0.1.0 — 文档版本与实现同步。*
