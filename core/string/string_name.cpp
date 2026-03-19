#include "core/string/string_name.h"

#include <cstring>
#include <mutex>
#include <unordered_map>

// FNV-1a 32-bit
static uint32_t
fnv1a32(const char* s, std::size_t len) noexcept
{
    uint32_t h = 2166136261u;
    for (std::size_t i = 0; i < len; ++i)
    {
        h ^= static_cast<uint8_t>(s[i]);
        h *= 16777619u;
    }
    return h;
}

struct StringName::Table
{
    std::mutex                             mtx;
    std::unordered_map<std::string, Data*> map;
};

StringName::Table&
StringName::get_table()
{
    static Table t;
    return t;
}

StringName::Data*
StringName::intern(const char* s, std::size_t len)
{
    if (!s || len == 0)
    {
        return nullptr;
    }

    auto&                       t = get_table();
    std::lock_guard<std::mutex> lock(t.mtx);

    std::string key(s, len);
    auto        it = t.map.find(key);
    if (it != t.map.end())
    {
        it->second->refcount.fetch_add(1, std::memory_order_relaxed);
        return it->second;
    }

    auto* d = new Data{};
    d->hash = fnv1a32(s, len);
    d->str  = std::move(key);
    t.map.emplace(d->str, d);
    return d;
}

void
StringName::unref(Data* d) noexcept
{
    if (!d)
    {
        return;
    }
    if (d->refcount.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        auto&                       t = get_table();
        std::lock_guard<std::mutex> lock(t.mtx);
        t.map.erase(d->str);
        delete d;
    }
}

StringName::StringName(const char* s)
{
    if (s)
    {
        _data = intern(s, std::strlen(s));
    }
}

StringName::StringName(const std::string& s)
{
    if (!s.empty())
    {
        _data = intern(s.c_str(), s.size());
    }
}

StringName::StringName(const StringName& other) noexcept : _data(other._data)
{
    if (_data)
    {
        _data->refcount.fetch_add(1, std::memory_order_relaxed);
    }
}

StringName::StringName(StringName&& other) noexcept : _data(other._data)
{
    other._data = nullptr;
}

StringName::~StringName()
{
    unref(_data);
}

StringName&
StringName::operator=(const StringName& other) noexcept
{
    if (this != &other)
    {
        unref(_data);
        _data = other._data;
        if (_data)
        {
            _data->refcount.fetch_add(1, std::memory_order_relaxed);
        }
    }
    return *this;
}

StringName&
StringName::operator=(StringName&& other) noexcept
{
    if (this != &other)
    {
        unref(_data);
        _data       = other._data;
        other._data = nullptr;
    }
    return *this;
}
