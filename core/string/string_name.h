#pragma once

#include <atomic>
#include <cstdint>
#include <string>

// Interned string — equality and hash are O(1) via pointer identity.
// Empty StringName (default-constructed) compares equal to other empty StringNames.
class StringName
{
    struct Data
    {
        std::atomic<uint32_t> refcount{1};
        uint32_t              hash = 0;
        std::string           str;
    };

    // Table is forward-declared; defined in string_name.cpp to avoid
    // pulling <mutex>/<unordered_map> into every translation unit.
    struct Table;

    Data* _data = nullptr;

    static Table& get_table();
    static Data*  intern(const char* s, std::size_t len);
    static void   unref(Data* d) noexcept;

public:
    StringName() = default;
    explicit StringName(const char* s);
    explicit StringName(const std::string& s);
    StringName(const StringName& other) noexcept;
    StringName(StringName&& other) noexcept;
    ~StringName();

    StringName& operator=(const StringName& other) noexcept;
    StringName& operator=(StringName&& other) noexcept;

    // Pointer comparison — valid because all equal strings share a single Data.
    [[nodiscard]] bool operator==(const StringName& o) const noexcept { return _data == o._data; }

    [[nodiscard]] bool operator!=(const StringName& o) const noexcept { return _data != o._data; }

    [[nodiscard]] bool operator<(const StringName& o) const noexcept { return _data < o._data; }

    [[nodiscard]] explicit operator bool() const noexcept { return _data != nullptr; }

    [[nodiscard]] const char* c_str() const noexcept { return _data ? _data->str.c_str() : ""; }

    [[nodiscard]] uint32_t hash() const noexcept { return _data ? _data->hash : 0u; }

    [[nodiscard]] std::size_t length() const noexcept { return _data ? _data->str.size() : 0u; }

    [[nodiscard]] bool empty() const noexcept { return _data == nullptr; }
};

namespace std
{
    template <>
    struct hash<StringName>
    {
        std::size_t operator()(const StringName& sn) const noexcept { return sn.hash(); }
    };
} // namespace std
