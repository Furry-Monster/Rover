# ADR-0012: 用互斥锁保护工作窃取队列的任务所有权

- **Status**: Accepted
- **Date**: 2026-11-05
- **Deciders**: core team

## Context

Phase 2 全量验证可独立复现 JobSystem 嵌套提交测试的 `std::bad_function_call` 崩溃。本地队列在最后一个元素的 CAS 之前移动 `std::function`，窃取者可能同时复制同一槽位；队列满时也会丢弃已经计入在途数量的任务。

## Decision

保留本地有界环形双端队列、所有者 LIFO 与窃取者 FIFO 的调度方式，用队列级互斥锁串行化槽位访问。新增 `try_push(T&)`，队列满时不移动参数，JobSystem 回退到全局队列。所有权仅在成功弹出或窃取时转移。

## Consequences

- 消除非平凡任务对象的并发读写与重复消费风险。
- 本地队列溢出时任务不会丢失，`wait_all()` 可以完成。
- 队列访问增加锁开销；未来若要重新实现无锁队列，须单独证明槽位生命周期并验证并发行为。

## References

- [任务系统](../../dev/CORE.md#83-workstealingqueue--work_stealing_queueh)
- [测试](../../../tests/core/task_test.cpp)
