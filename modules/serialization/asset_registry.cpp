#include "modules/serialization/asset_registry.h"

#include <utility>

namespace rover
{

    AssetRegistry& AssetRegistry::get()
    {
        static AssetRegistry instance;
        return instance;
    }

    AssetID AssetRegistry::register_asset(AssetKind kind, std::string virtual_path, std::string source_path)
    {
        auto pit = by_path_.find(virtual_path);
        if (pit != by_path_.end())
        {
            // Already registered -- patch metadata in place so callers refresh
            // the source_path when re-importing.
            auto& entry       = by_id_[pit->second];
            entry.kind        = kind;
            entry.source_path = std::move(source_path);
            return pit->second;
        }

        AssetEntry entry{};
        entry.id           = next_id_++;
        entry.kind         = kind;
        entry.virtual_path = std::move(virtual_path);
        entry.source_path  = std::move(source_path);

        const AssetID id             = entry.id;
        by_path_[entry.virtual_path] = id;
        by_id_[id]                   = std::move(entry);
        return id;
    }

    bool AssetRegistry::unregister(AssetID id)
    {
        auto it = by_id_.find(id);
        if (it == by_id_.end())
        {
            return false;
        }
        by_path_.erase(it->second.virtual_path);
        by_id_.erase(it);
        return true;
    }

    const AssetEntry* AssetRegistry::find(AssetID id) const
    {
        auto it = by_id_.find(id);
        return it == by_id_.end() ? nullptr : &it->second;
    }

    AssetID AssetRegistry::find_id(const std::string& virtual_path) const
    {
        auto it = by_path_.find(virtual_path);
        return it == by_path_.end() ? INVALID_ASSET_ID : it->second;
    }

    void AssetRegistry::clear()
    {
        by_path_.clear();
        by_id_.clear();
        next_id_ = 1;
    }

} // namespace rover
