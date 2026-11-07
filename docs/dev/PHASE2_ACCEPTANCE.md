# Phase 2 验收

目标：完成 v0.2 的 ECS 场景、保存加载、GPU 渲染及资源管线，并完成已列入 Phase 2 的工具与工程收尾。依据 [路线图](../product/ROADMAP.md)、[需求](../product/REQUIREMENTS.md) 和资源版本完成后的开发顺序逐项验收。

## 验收清单

| 项目 | 完成证据 | 状态 |
|------|----------|------|
| ECS、场景树、相机与光照，JSON/RBIN 保存加载并恢复网格 | 场景/序列化真实文件往返测试及五个场景 GPU 回归 | 已有实现，最终复验 |
| Variant、Callable、文件系统抽象 | 对应 core/platform 测试；同步 F-111/F-208/F-209 状态 | 已有实现，最终复验 |
| 绑定、push constant、staging、纹理上传 | driver 与模块测试及真实 shader/上传 GPU 使用 | 已有实现，最终复验 |
| Frame Graph DAG、版本覆盖、自动同步 | 版本排列测试与双版本 GPU 回读 | 已有实现，最终复验 |
| 瞬态纹理/缓冲、使用区间与资源池 | 非重叠区间复用、在途帧隔离、失败回滚、回收、尺寸变化测试；后处理与相机迁移 | 待完成 |
| mip/layer/aspect 子资源跟踪 | 不同范围独立状态、重叠范围依赖、整资源兼容性、实际 GPU 子资源验证 | 待完成 |
| Vulkan 头文件 vendoring | 固定源码与校验记录，第一方 CMake 移除 find_package(Vulkan)，干净配置构建 | 待完成 |
| F-605 lint 命令 | CLI、clang-tidy 调用与文件筛选测试，实际调用检查 | 待完成 |
| 工程验收 | Debug/Release 构建、单元测试、ASan/UBSan、可重复 GPU 回归脚本，记录环境和结果 | 待完成 |
| 里程碑与需求同步 | 上述证据逐项核验后更新 Roadmap 和 Requirements | 待完成 |

外部模型/图片导入、材质与纹理的磁盘引用、磁盘资产注册表和编辑器属于路线图中的 Phase 3。显存别名、异步队列、pass 剔除和 shader 优化作为后续增强。

## 资源生命周期原则

- 池中的资源只有在其最后使用提交完成后，才能交给下一帧。
- 同一图内描述相同、使用区间不相交的 GPU 资源可以复用同一物理对象；主机可见缓冲保留整帧租约。
- framebuffer 与 bind group 由池缓存，清理早于引用的纹理/缓冲和布局。
- 分配或绑定创建失败，撤销候选资源，已存在的池对象与旧渲染配置保持有效。
- 设备关闭前等待 GPU，并按绑定、framebuffer、资源、pipeline/layout/render pass 的依赖顺序释放。
