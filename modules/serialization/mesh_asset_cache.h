#pragma once

#include "modules/scene/components/mesh_component.h"

#include <string>
#include <unordered_map>

namespace rover
{

    class GraphicsDevice;
    class World;

    // Owns shared GPU buffers for the built-in mesh paths used by Phase 2 scenes.
    // The device must outlive the cache. World components only borrow its handles.
    class MeshAssetCache
    {
    public:
        explicit MeshAssetCache(GraphicsDevice& device);
        ~MeshAssetCache();

        MeshAssetCache(const MeshAssetCache&)            = delete;
        MeshAssetCache& operator=(const MeshAssetCache&) = delete;

        // On failure no component is changed. Successfully uploaded assets remain
        // cached until clear/destruction, including uploads preceding a failure.
        bool resolve(World& world);

        // Waits for outstanding GPU work before releasing all owned buffers.
        // Borrowed component handles become invalid and must be resolved again.
        void clear();

    private:
        GraphicsDevice&                                device_;
        std::unordered_map<std::string, MeshComponent> meshes_;
    };

} // namespace rover
