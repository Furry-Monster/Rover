#include "modules/serialization/texture_uploader.h"

#include "core/log/log.h"

namespace rover
{

    TextureGpuHandles TextureUploader::upload(GraphicsDevice& device, const TextureData& data, const char* debug_name)
    {
        TextureGpuHandles out{};
        if (data.pixels.empty() || data.width == 0 || data.height == 0)
        {
            ROVER_LOG_WARN("TextureUploader: empty texture data");
            return out;
        }

        TextureDesc tex_desc{};
        tex_desc.width        = data.width;
        tex_desc.height       = data.height;
        tex_desc.depth        = 1;
        tex_desc.mip_levels   = 1;
        tex_desc.array_layers = 1;
        tex_desc.format       = data.format;
        tex_desc.type         = TextureType::Texture2D;
        tex_desc.usage        = TextureUsage::Sampled | TextureUsage::TransferDst;
        tex_desc.debug_name   = debug_name;
        out.texture           = device.create_texture(tex_desc);
        if (out.texture == INVALID_HANDLE)
        {
            ROVER_LOG_ERROR("TextureUploader: create_texture failed");
            return TextureGpuHandles{};
        }
        device.update_texture(out.texture, data.pixels.data(), data.byte_size());

        SamplerDesc sd{};
        sd.min_filter     = Filter::Linear;
        sd.mag_filter     = Filter::Linear;
        sd.address_u      = SamplerAddressMode::Repeat;
        sd.address_v      = SamplerAddressMode::Repeat;
        sd.address_w      = SamplerAddressMode::Repeat;
        sd.max_anisotropy = 1.0f;
        out.sampler       = device.create_sampler(sd);
        if (out.sampler == INVALID_HANDLE)
        {
            ROVER_LOG_ERROR("TextureUploader: create_sampler failed");
            device.destroy_texture(out.texture);
            return TextureGpuHandles{};
        }
        return out;
    }

    void TextureUploader::destroy(GraphicsDevice& device, TextureGpuHandles& handles)
    {
        if (handles.sampler != INVALID_HANDLE)
        {
            device.destroy_sampler(handles.sampler);
            handles.sampler = INVALID_HANDLE;
        }
        if (handles.texture != INVALID_HANDLE)
        {
            device.destroy_texture(handles.texture);
            handles.texture = INVALID_HANDLE;
        }
    }

} // namespace rover
