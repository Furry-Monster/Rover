# ADR-0009: Variant 实现策略

- **Status**: Accepted
- **Date**: 2026-04-29
- **Deciders**: core team

## Context

Phase 2 引入 `core/variant/Variant`，作为反射、序列化、编辑器 CLI、事件参数等场景的「**类型擦除值**」载体。Variant 在引擎中的调用路径几乎都是**热路径**（每帧 N 次的 component 序列化、Inspector 取值、Callable 转参数），其值语义、内存布局、拷贝成本都直接影响整体表现。

候选实现路径：

1. **手写 union + type tag**（Godot 的 `core/variant/variant.h` 路线）：完全控制内存布局，可对小类型 inline 存放、对大类型走指针 / 引用计数。
2. **`std::variant<T...>`**：标准库提供的安全 tagged union，但生成代码体积较大、不能轻易 inline 大类型，且对前向声明、Object 指针等需要包装。
3. **纯 SBO（Small Buffer Optimization）**：自定义 buffer 内联存储 + 类型函数指针表（vtable）。这是 `std::any` 的方式，写作会非常类似 `inline_function`。

引擎对 Variant 的硬性要求：

- 最常用类型（Bool / Int / Float / Vector{2,3,4} / Quat）必须**完全 inline**，零堆分配；
- 大类型（Mat4 = 64B）至少其中一种实现要可 inline，因为相机矩阵、Transform.to_mat4() 取值非常频繁；
- 字符串 / Array / Dictionary 必须**共享语义**，避免每次拷贝都触发深拷贝；
- 不依赖 RTTI（项目用 `-fno-rtti`）；
- 拷贝/移动/析构必须是 C++ trivially-noexcept-friendly，使 Variant 自己能放进 `std::vector` / `std::unordered_map`。

## Decision

采用 **手写 union + type tag + 64 字节 inline 缓冲区**。

具体细节：

- `Variant::Type` 是单字节 enum；`Variant` 整体大小 ≤ 80 字节（实测 72-80 之间，取决于 `shared_ptr` 在目标平台的对齐）；
- 所有 POD 类型（Bool / Int / Float / Vector{2,3,4} / Quat / Mat4 / Color / Object*）**完全 inline**；
- String / Array / Dictionary 通过 placement-new 在 union 中构造 `std::shared_ptr<…>`，析构时显式调用 `~shared_ptr()`，实现「共享拷贝、深 mutate」；
- 通过 `copy_from` / `move_from` 内部函数处理类型分支，避免在头文件中暴露所有类型构造分支。

`std::variant` 没被采用的原因：

- 不能内联超过 8 个候选类型 + 大类型时编译时间显著膨胀；
- `std::variant<X, Y>` 的 ABI 不可控，跨二进制（未来 GDExtension 风格的插件接口）不友好；
- 对 `Object*` 这种非默认构造类型在 union 里要套一层 `std::optional`，再加一层间接。

纯 SBO 没被采用的原因：

- 16-byte SBO 不足以 inline Mat4，需要 64-byte SBO，已经接近完整 union；
- 需要为每种类型生成 dispatch 表，编译期工作量与 union+tag 相当但调试观察更复杂。

## Consequences

**Positive:**

- 所有热路径访问 Variant 是 O(1)，绝大多数零分配；
- Mat4 inline 让相机/Transform 路径完全无堆压力；
- String / Array / Dictionary 的 `shared_ptr` 路径让序列化时拷贝廉价（仅原子计数）；
- 与 Godot Variant 语义对齐，未来若需要跨引擎对比 / 借鉴优化容易迁移；
- 实现完全在我们控制下，未来添加 NodePath / RID / Callable 之类的内置类型只需扩 enum + 加分支。

**Negative / Trade-offs:**

- 手写 union + placement new 必须保证 copy/move/clear 的每一条 case 路径正确，否则会触发 use-after-free（已通过 `tests/core/variant_test.cpp` 覆盖）；
- 80 字节比 16 字节大，传值时开销稍高（实测 register-pass-by-value 不再可行），但通常都按 const&/&& 传；
- 后续如果要序列化「自定义类型」（如自定义 Resource 子类），需要走 `Object*` + ClassDB 而非直接塞进 Variant。

## Alternatives Considered

- `std::variant<T...>`：放弃理由见上。
- `std::any`：完全擦除类型 → 反射时拿不到 `Type` enum，与序列化模块需求冲突。
- 引用计数包装（每个 Variant = `RefCounted<Inner>*`）：所有访问要走一次解引用，零拷贝代价转嫁到访问，不利于 hot path。

## References

- 实现：[core/variant/variant.h](../../../core/variant/variant.h), [core/variant/variant.cpp](../../../core/variant/variant.cpp)
- 类型转换辅助：[core/variant/variant_convert.h](../../../core/variant/variant_convert.h)
- Callable：[core/variant/callable.h](../../../core/variant/callable.h)
- Godot 参考：`godot/core/variant/variant.h`
- 单元测试：[tests/core/variant_test.cpp](../../../tests/core/variant_test.cpp)
- 关联需求：[F-208 / F-209](../REQUIREMENTS.md)
