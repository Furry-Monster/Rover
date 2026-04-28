#pragma once

#include "core/typedefs.h"

#include <unordered_map>
#include <utility>

namespace rover {

template <typename T>
class VkResourcePool {
public:
    u64 add(T resource) {
        const u64 handle = next_handle_++;
        resources_.emplace(handle, std::move(resource));
        return handle;
    }

    T* get(u64 handle) {
        if (handle == 0) return nullptr;
        auto it = resources_.find(handle);
        return it != resources_.end() ? &it->second : nullptr;
    }

    const T* get(u64 handle) const {
        if (handle == 0) return nullptr;
        auto it = resources_.find(handle);
        return it != resources_.end() ? &it->second : nullptr;
    }

    bool remove(u64 handle, T* out_removed = nullptr) {
        if (handle == 0) return false;
        auto it = resources_.find(handle);
        if (it == resources_.end()) return false;
        if (out_removed) *out_removed = std::move(it->second);
        resources_.erase(it);
        return true;
    }

    template <typename Fn>
    void for_each(Fn&& fn) {
        for (auto& [handle, resource] : resources_) {
            fn(handle, resource);
        }
    }

    void clear() { resources_.clear(); }

    [[nodiscard]] usize size() const { return resources_.size(); }

private:
    std::unordered_map<u64, T> resources_;
    u64                        next_handle_ = 1;
};

} // namespace rover
