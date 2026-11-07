#pragma once

#include "core/graphics/graphics_types.h"

#include <algorithm>
#include <limits>

namespace rover
{
    enum class TextureAspect : u8
    {
        Automatic    = 0,
        Color        = 1,
        Depth        = 2,
        Stencil      = 4,
        DepthStencil = 6,
    };
    ROVER_DEFINE_FLAG_OPERATORS(TextureAspect)

    struct TextureSubresourceRange
    {
        TextureAspect aspects     = TextureAspect::Automatic;
        u32           base_mip    = 0;
        u32           mip_count   = 0; // Remaining levels.
        u32           base_layer  = 0;
        u32           layer_count = 0; // Remaining layers (cube faces count as layers).
        bool          operator==(const TextureSubresourceRange&) const = default;
    };

    constexpr TextureSubresourceRange TEXTURE_BASE_SUBRESOURCE{TextureAspect::Automatic, 0, 1, 0, 1};

    struct TextureCopyRegion
    {
        TextureSubresourceRange range         = TEXTURE_BASE_SUBRESOURCE;
        usize                   buffer_offset = 0;
    };

    constexpr TextureAspect texture_aspects(Format format)
    {
        switch (format)
        {
            case Format::D32_SFLOAT:
                return TextureAspect::Depth;
            case Format::D24_UNORM_S8_UINT:
            case Format::D32_SFLOAT_S8_UINT:
                return TextureAspect::Depth | TextureAspect::Stencil;
            case Format::UNDEFINED:
                return TextureAspect::Automatic;
            default:
                return TextureAspect::Color;
        }
    }

    constexpr bool normalize_texture_range(Format                   format,
                                           u32                      mips,
                                           u32                      layers,
                                           TextureSubresourceRange  input,
                                           TextureSubresourceRange& result)
    {
        const auto available = texture_aspects(format);
        if (input.aspects == TextureAspect::Automatic)
        {
            input.aspects = available;
        }
        if (input.aspects == TextureAspect::Automatic ||
            (static_cast<u8>(input.aspects) & ~static_cast<u8>(available)) != 0 || input.base_mip >= mips ||
            input.base_layer >= layers)
        {
            return false;
        }
        if (input.mip_count == 0)
        {
            input.mip_count = mips - input.base_mip;
        }
        if (input.layer_count == 0)
        {
            input.layer_count = layers - input.base_layer;
        }
        if (input.mip_count > mips - input.base_mip || input.layer_count > layers - input.base_layer)
        {
            return false;
        }
        result = input;
        return true;
    }

    constexpr u32 texture_aspect_index(TextureAspect aspect)
    {
        return aspect == TextureAspect::Color ? 0 : aspect == TextureAspect::Depth ? 1 : 2;
    }

    // Three fixed aspect planes make indices stable regardless of format.
    constexpr usize texture_cell_index(TextureAspect aspect, u32 mip, u32 layer, u32 mips, u32 layers)
    {
        return (static_cast<usize>(texture_aspect_index(aspect)) * layers + layer) * mips + mip;
    }

    template <typename Fn>
    void for_texture_cells(const TextureSubresourceRange& range, Fn&& fn)
    {
        for (auto aspect : {TextureAspect::Color, TextureAspect::Depth, TextureAspect::Stencil})
        {
            if ((range.aspects & aspect) == aspect)
            {
                for (u32 layer = range.base_layer; layer < range.base_layer + range.layer_count; ++layer)
                {
                    for (u32 mip = range.base_mip; mip < range.base_mip + range.mip_count; ++mip)
                    {
                        fn(aspect, mip, layer);
                    }
                }
            }
        }
    }

    constexpr bool texture_ranges_overlap(const TextureSubresourceRange& a, const TextureSubresourceRange& b)
    {
        return (a.aspects & b.aspects) != TextureAspect::Automatic && a.base_mip < b.base_mip + b.mip_count &&
               b.base_mip < a.base_mip + a.mip_count && a.base_layer < b.base_layer + b.layer_count &&
               b.base_layer < a.base_layer + a.layer_count;
    }

    constexpr u32 texture_mip_extent(u32 extent, u32 mip)
    {
        return mip >= 32 ? 1 : std::max(1u, extent >> mip);
    }

    constexpr u32 texture_plane_bytes(Format format, TextureAspect aspect)
    {
        if (aspect == TextureAspect::Stencil && (texture_aspects(format) & aspect) == aspect)
        {
            return 1;
        }
        if (aspect == TextureAspect::Depth && (texture_aspects(format) & aspect) == aspect)
        {
            return 4;
        }
        if (aspect != TextureAspect::Color)
        {
            return 0;
        }
        switch (format)
        {
            case Format::R8G8B8A8_UNORM:
            case Format::R8G8B8A8_SRGB:
            case Format::B8G8R8A8_UNORM:
            case Format::B8G8R8A8_SRGB:
                return 4;
            default:
                return 0;
        }
    }
} // namespace rover
