# 场景加载验证记录

**日期**：2026-11-05  
**范围**：Phase 2 场景加载、网格资源恢复与相关回归检查  
**结果**：构建、测试与 GPU 场景验证通过

## 验证结果

| 检查 | 当日结果 |
|------|----------|
| Debug 构建 | 成功 |
| 全量测试 | 128 个测试、777 条断言通过 |
| 任务队列与 JobSystem | 连续运行 25 次通过 |
| GPU 场景加载 | Intel Iris Xe / Vulkan 1.3，启用验证层，首次创建和修改后重载各完成 5 帧 |
| 加载失败处理 | 未知资源和损坏 JSON 按预期退出，保留原文件 |
| Vulkan 验证 | 未记录验证层 error |

## 环境与工程检查

初始化日志提示 `VK_KHR_get_physical_device_properties2` 被列为 device extension 的 warning。该条保留为当日环境记录。

当日修改的 C++ 文件通过格式检查；全库格式检查报告以下文件存在格式差异：

- `core/graphics/graphics_device.h`
- `core/variant/callable.h`
- `drivers/vulkan/vk_instance.cpp`
- `tests/core/event_test.cpp`

当前运行步骤和测试入口见 [场景加载](../dev/SCENE_LOADING.md)，资源路径与所有权设计见 [ADR-0011](../product/adr/ADR-0011-scene-mesh-resource-paths.md)。
