#include "modules/serialization/mesh_asset_cache.h"

#include "core/log/log.h"
#include "modules/scene/world.h"
#include "modules/serialization/mesh_uploader.h"
#include "modules/serialization/primitive_meshes.h"

namespace rover
{

    MeshAssetCache::MeshAssetCache(GraphicsDevice& device) : device_(device) {}

    MeshAssetCache::~MeshAssetCache()
    {
        clear();
    }

    bool MeshAssetCache::resolve(World& world)
    {
        auto view = world.view<MeshComponent>();
        for (auto entity : view)
        {
            const auto& path = world.get_component<MeshComponent>(entity)->mesh_asset;
            if (meshes_.contains(path))
            {
                continue;
            }

            MeshData data;
            if (path == "builtin://meshes/cube")
            {
                data = make_cube();
            }
            else if (path == "builtin://meshes/quad")
            {
                data = make_quad();
            }
            else if (path == "builtin://meshes/sphere")
            {
                data = make_sphere();
            }
            else
            {
                ROVER_LOG_ERROR("MeshAssetCache: missing or unsupported mesh_asset '{}'", path);
                return false;
            }

            auto mesh = MeshUploader::upload(device_, data, path.c_str());
            if (mesh.vertex_buffer == INVALID_HANDLE)
            {
                ROVER_LOG_ERROR("MeshAssetCache: failed to upload '{}'", path);
                return false;
            }
            mesh.mesh_asset = path;
            meshes_.emplace(path, std::move(mesh));
        }

        // Commit only after every reference resolves, so callers never receive a
        // World with a mixture of old and newly resolved component handles.
        for (auto entity : view)
        {
            auto& mesh = *world.get_component<MeshComponent>(entity);
            mesh       = meshes_.at(mesh.mesh_asset);
        }
        return true;
    }

    void MeshAssetCache::clear()
    {
        if (meshes_.empty())
        {
            return;
        }
        device_.wait_idle();
        for (auto& [path, mesh] : meshes_)
        {
            MeshUploader::destroy_buffers(device_, mesh);
        }
        meshes_.clear();
    }

} // namespace rover
