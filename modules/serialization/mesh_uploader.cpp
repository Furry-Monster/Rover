#include "modules/serialization/mesh_uploader.h"

#include "core/log/log.h"

namespace rover
{

    MeshComponent MeshUploader::upload(GraphicsDevice& device, const MeshData& data, const char* debug_name)
    {
        MeshComponent mc{};
        if (data.vertices.empty())
        {
            ROVER_LOG_WARN("MeshUploader: empty mesh data");
            return mc;
        }

        BufferDesc vb_desc{};
        vb_desc.size       = data.vertex_data_size();
        vb_desc.usage      = BufferUsage::Vertex;
        vb_desc.memory     = MemoryUsage::GpuOnly;
        vb_desc.debug_name = debug_name;
        mc.vertex_buffer   = device.create_buffer(vb_desc);
        if (mc.vertex_buffer == INVALID_HANDLE)
        {
            ROVER_LOG_ERROR("MeshUploader: failed to create vertex buffer");
            return MeshComponent{};
        }
        device.update_buffer(mc.vertex_buffer, data.vertices.data(), vb_desc.size);
        mc.vertex_count = data.vertex_count();

        if (!data.indices.empty())
        {
            BufferDesc ib_desc{};
            ib_desc.size       = data.index_data_size();
            ib_desc.usage      = BufferUsage::Index;
            ib_desc.memory     = MemoryUsage::GpuOnly;
            ib_desc.debug_name = debug_name;
            mc.index_buffer    = device.create_buffer(ib_desc);
            if (mc.index_buffer == INVALID_HANDLE)
            {
                ROVER_LOG_ERROR("MeshUploader: failed to create index buffer");
                device.destroy_buffer(mc.vertex_buffer);
                return MeshComponent{};
            }
            device.update_buffer(mc.index_buffer, data.indices.data(), ib_desc.size);
            mc.index_count = data.index_count();
            mc.index_type  = IndexType::Uint32;
        }
        return mc;
    }

    void MeshUploader::destroy_buffers(GraphicsDevice& device, MeshComponent& mc)
    {
        if (mc.vertex_buffer != INVALID_HANDLE)
        {
            device.destroy_buffer(mc.vertex_buffer);
            mc.vertex_buffer = INVALID_HANDLE;
        }
        if (mc.index_buffer != INVALID_HANDLE)
        {
            device.destroy_buffer(mc.index_buffer);
            mc.index_buffer = INVALID_HANDLE;
        }
        mc.vertex_count = 0;
        mc.index_count  = 0;
    }

} // namespace rover
