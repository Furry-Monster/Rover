#pragma once

#include "core/string/string_name.h"

#include <functional>

class Object;
class Variant;

/**
 * @brief
 *
 * Callable — wraps either an Object+method pair or a standalone function.
 * Used as the target of signal connections.
 *
 * Object+method connections delegate to Object::call() (virtual dispatch).
 * Function connections invoke the stored std::function directly.
 */
class Callable
{
public:
    using FnType = std::function<void(const Variant*, int)>;

    Callable() = default;

    Callable(Object* p_object, const StringName& p_method);

    explicit Callable(FnType p_fn);

    void call(const Variant* p_args, int p_arg_count) const;

    [[nodiscard]] bool is_valid() const;

    [[nodiscard]] Object* get_object() const { return _object; }

    [[nodiscard]] const StringName& get_method() const { return _method; }

    bool operator==(const Callable& p_other) const;

    bool operator!=(const Callable& p_other) const { return !(*this == p_other); }

private:
    Object*    _object = nullptr;
    StringName _method;
    FnType     _custom_fn;
};
