#include "core/object/callable.h"

#include "core/object/object.h"
#include "core/variant/variant.h"

Callable::Callable(Object* p_object, const StringName& p_method) : _object(p_object), _method(p_method) {}

Callable::Callable(FnType p_fn) : _custom_fn(std::move(p_fn)) {}

void
Callable::call(const Variant* p_args, int p_arg_count) const
{
    if (_custom_fn)
    {
        _custom_fn(p_args, p_arg_count);
    }
    else if (_object && _method)
    {
        _object->call(_method, p_args, p_arg_count);
    }
}

bool
Callable::is_valid() const
{
    return _custom_fn || (_object != nullptr && _method);
}

bool
Callable::operator==(const Callable& p_other) const
{
    if (_object && p_other._object)
    {
        return _object == p_other._object && _method == p_other._method;
    }
    // Function callables are compared by identity (always unequal).
    return false;
}
