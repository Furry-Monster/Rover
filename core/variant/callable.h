#pragma once

#include "core/variant/variant.h"

#include <functional>
#include <memory>
#include <utility>

namespace rover
{

    // ---------------------------------------------------------------------------
    // Callable: type-erased callable wrapper.
    //
    // Wraps any function-like (free function / lambda / member function) into a
    // uniform `(VariantArray args) -> Variant` shape. Used for Variant-driven
    // invocation in the editor CLI, scripting integration, and event dispatch.
    // ---------------------------------------------------------------------------
    class Callable
    {
    public:
        using Function = std::function<Variant(const VariantArray&)>;

        Callable() = default;

        // Construct from anything callable with `(const VariantArray&) -> Variant`.
        Callable(Function fn) : fn_(std::move(fn)) {}

        [[nodiscard]] bool is_valid() const noexcept { return static_cast<bool>(fn_); }

        Variant call(const VariantArray& args) const
        {
            if (!fn_)
            {
                return {};
            }
            return fn_(args);
        }

        Variant operator()(const VariantArray& args) const { return call(args); }

        // ---- Convenience adapters ----

        // Wrap a free function `Variant(*)(const VariantArray&)`.
        static Callable from_function(Variant (*fn)(const VariantArray&)) { return Callable{Function(fn)}; }

        // Wrap a lambda or functor that takes (const VariantArray&) and returns Variant.
        template <typename F>
        static Callable from_lambda(F&& fn)
        {
            return Callable{Function(std::forward<F>(fn))};
        }

        // Wrap a member function pointer of `T::method(const VariantArray&) -> Variant`.
        template <typename T>
        static Callable from_method(T* instance, Variant (T::*method)(const VariantArray&))
        {
            return Callable{[instance, method](const VariantArray& args) {
                return (instance->*method)(args);
            }};
        }

    private:
        Function fn_;
    };

} // namespace rover
