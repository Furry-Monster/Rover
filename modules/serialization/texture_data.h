#pragma once

#include "core/graphics/graphics_types.h"
#include "core/typedefs.h"

#include <vector>

namespace rover
{

    // CPU-side texture representation. Generators (procedural patterns, future
    // PNG/KTX2 decoders) populate `TextureData`; `TextureUploader` then promotes
    // it to a GPU texture + sampler via the abstract `GraphicsDevice` API.
    struct TextureData
    {
        u32             width  = 0;
        u32             height = 0;
        Format          format = Format::R8G8B8A8_SRGB;
        std::vector<u8> pixels; // tightly packed, row-major

        [[nodiscard]] usize byte_size() const noexcept { return pixels.size(); }
    };

    // ---------------------------------------------------------------------------
    // Procedural texture generators (used until stb_image / KTX-Software are
    // vendored per ADR-0008). These guarantee the GPU upload path is exercised
    // without needing external file fixtures.
    // ---------------------------------------------------------------------------

    // Solid-colored RGBA texture.
    [[nodiscard]] TextureData make_solid_rgba(u32 width, u32 height, u8 r, u8 g, u8 b, u8 a = 255);

    // Checkerboard pattern with two RGBA colors and N pixels per cell.
    [[nodiscard]] TextureData
    make_checkerboard(u32 width, u32 height, u32 cell_size, u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2);

} // namespace rover
