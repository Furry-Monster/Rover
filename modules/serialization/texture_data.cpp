#include "modules/serialization/texture_data.h"

namespace rover
{

    TextureData make_solid_rgba(u32 width, u32 height, u8 r, u8 g, u8 b, u8 a)
    {
        TextureData t;
        t.width  = width;
        t.height = height;
        t.format = Format::R8G8B8A8_SRGB;
        t.pixels.resize(static_cast<usize>(width) * height * 4);
        for (usize i = 0; i < t.pixels.size(); i += 4)
        {
            t.pixels[i + 0] = r;
            t.pixels[i + 1] = g;
            t.pixels[i + 2] = b;
            t.pixels[i + 3] = a;
        }
        return t;
    }

    TextureData make_checkerboard(u32 width, u32 height, u32 cell_size, u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2)
    {
        if (cell_size == 0)
        {
            cell_size = 1;
        }
        TextureData t;
        t.width  = width;
        t.height = height;
        t.format = Format::R8G8B8A8_SRGB;
        t.pixels.resize(static_cast<usize>(width) * height * 4);
        for (u32 y = 0; y < height; ++y)
        {
            for (u32 x = 0; x < width; ++x)
            {
                const u32  cx     = x / cell_size;
                const u32  cy     = y / cell_size;
                const bool first  = ((cx + cy) & 1u) == 0u;
                const u32  idx    = (y * width + x) * 4u;
                t.pixels[idx + 0] = first ? r1 : r2;
                t.pixels[idx + 1] = first ? g1 : g2;
                t.pixels[idx + 2] = first ? b1 : b2;
                t.pixels[idx + 3] = 255;
            }
        }
        return t;
    }

} // namespace rover
