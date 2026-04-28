#pragma once

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace rover {

template<typename Signature>
class Delegate;

/// Lightweight type-erased callable wrapper with small-buffer optimization.
/// Avoids heap allocation for callables up to sbo_size bytes.
/// Move-only — copying is deleted to prevent accidental deep copies.
template<typename Ret, typename... Args>
class Delegate<Ret(Args...)> {
    static constexpr std::size_t sbo_size  = 32;
    static constexpr std::size_t sbo_align = alignof(std::max_align_t);

    using InvokeFn  = Ret(*)(void*, Args...);
    using DestroyFn = void(*)(void*);
    using MoveFn    = void(*)(void*, void*);

    alignas(sbo_align) unsigned char storage_[sbo_size]{};
    InvokeFn  invoke_  = nullptr;
    DestroyFn destroy_ = nullptr;
    MoveFn    move_fn_ = nullptr;
    bool      on_heap_ = false;

public:
    Delegate() noexcept = default;
    Delegate(std::nullptr_t) noexcept {}

    /// Construct from any compatible callable (lambda, functor, function pointer).
    template<typename F>
        requires (!std::is_same_v<std::decay_t<F>, Delegate>)
              && std::is_invocable_r_v<Ret, std::decay_t<F>&, Args...>
    Delegate(F&& f) {
        using Callable = std::decay_t<F>;

        constexpr bool use_sbo = sizeof(Callable) <= sbo_size
                              && alignof(Callable) <= sbo_align
                              && std::is_nothrow_move_constructible_v<Callable>;

        if constexpr (use_sbo) {
            ::new (storage_) Callable(std::forward<F>(f));
            on_heap_ = false;

            invoke_ = [](void* s, Args... args) -> Ret {
                return (*static_cast<Callable*>(s))(std::forward<Args>(args)...);
            };
            destroy_ = [](void* s) {
                static_cast<Callable*>(s)->~Callable();
            };
            move_fn_ = [](void* dst, void* src) {
                ::new (dst) Callable(std::move(*static_cast<Callable*>(src)));
                static_cast<Callable*>(src)->~Callable();
            };
        } else {
            auto* ptr = new Callable(std::forward<F>(f));
            std::memcpy(storage_, &ptr, sizeof(ptr));
            on_heap_ = true;

            invoke_ = [](void* s, Args... args) -> Ret {
                return (**static_cast<Callable**>(s))(std::forward<Args>(args)...);
            };
            destroy_ = [](void* s) {
                delete *static_cast<Callable**>(s);
            };
            move_fn_ = [](void* dst, void* src) {
                std::memcpy(dst, src, sizeof(Callable*));
                *static_cast<Callable**>(src) = nullptr;
            };
        }
    }

    ~Delegate() { reset(); }

    Delegate(Delegate&& other) noexcept
        : invoke_(other.invoke_)
        , destroy_(other.destroy_)
        , move_fn_(other.move_fn_)
        , on_heap_(other.on_heap_)
    {
        if (other.invoke_ && move_fn_) {
            move_fn_(storage_, other.storage_);
        }
        other.invoke_  = nullptr;
        other.destroy_ = nullptr;
        other.move_fn_ = nullptr;
        other.on_heap_ = false;
    }

    Delegate& operator=(Delegate&& other) noexcept {
        if (this != &other) {
            reset();
            invoke_  = other.invoke_;
            destroy_ = other.destroy_;
            move_fn_ = other.move_fn_;
            on_heap_ = other.on_heap_;
            if (other.invoke_ && move_fn_) {
                move_fn_(storage_, other.storage_);
            }
            other.invoke_  = nullptr;
            other.destroy_ = nullptr;
            other.move_fn_ = nullptr;
            other.on_heap_ = false;
        }
        return *this;
    }

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    /// Invoke the stored callable. Undefined behavior if empty.
    Ret operator()(Args... args) {
        return invoke_(static_cast<void*>(storage_), std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept { return invoke_ != nullptr; }

    void reset() noexcept {
        if (invoke_ && destroy_) {
            destroy_(static_cast<void*>(storage_));
        }
        invoke_  = nullptr;
        destroy_ = nullptr;
        move_fn_ = nullptr;
        on_heap_ = false;
    }

    /// Create a Delegate from a compile-time free function pointer.
    template<auto FnPtr>
    static Delegate from_function()
        requires std::is_invocable_r_v<Ret, decltype(FnPtr), Args...>
    {
        Delegate d;
        d.invoke_ = [](void*, Args... args) -> Ret {
            return FnPtr(std::forward<Args>(args)...);
        };
        return d;
    }

    /// Create a Delegate from a compile-time member function pointer + object.
    /// The Delegate does NOT own the pointed-to object.
    template<auto MethodPtr, typename T>
    static Delegate from_method(T* obj)
        requires std::is_invocable_r_v<Ret, decltype(MethodPtr), T*, Args...>
    {
        static_assert(sizeof(T*) <= sbo_size, "Object pointer must fit in SBO storage");

        Delegate d;
        std::memcpy(d.storage_, &obj, sizeof(T*));

        d.invoke_ = [](void* s, Args... args) -> Ret {
            T* self;
            std::memcpy(&self, s, sizeof(T*));
            return (self->*MethodPtr)(std::forward<Args>(args)...);
        };
        d.move_fn_ = [](void* dst, void* src) {
            std::memcpy(dst, src, sizeof(T*));
        };
        return d;
    }
};

} // namespace rover
