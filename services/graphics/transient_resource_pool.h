#pragma once

#include "core/graphics/graphics_device.h"

#include <vector>

namespace rover
{
    class FrameGraph;

    // Owns immutable bindings/framebuffers and GPU resources. Leases are retired
    // by graphics submission serial, not by CPU frame count or swapchain index.
    class TransientResourcePool
    {
    public:
        explicit TransientResourcePool(GraphicsDevice& device) : device_(device) {}

        ~TransientResourcePool();
        TransientResourcePool(const TransientResourcePool&)            = delete;
        TransientResourcePool& operator=(const TransientResourcePool&) = delete;

        bool prepare(FrameGraph& graph);
        void clear(); // GPU idle, bindings -> framebuffers -> textures/buffers.

        [[nodiscard]] usize texture_count() const { return textures_.size(); }

        [[nodiscard]] usize buffer_count() const { return buffers_.size(); }

    private:
        friend class FrameGraph;
        u64 generation_ = 1;

        struct Texture
        {
            TextureDesc   desc{};
            TextureHandle handle  = INVALID_HANDLE;
            u64           retired = 0;
        };

        struct Buffer
        {
            BufferDesc   desc{};
            BufferHandle handle  = INVALID_HANDLE;
            u64          retired = 0;
        };

        struct Framebuffer
        {
            FramebufferDesc   desc{};
            FramebufferHandle handle = INVALID_HANDLE;
        };

        struct Bindings
        {
            BindGroupDesc   desc{};
            BindGroupHandle handle = INVALID_HANDLE;
        };

        GraphicsDevice&          device_;
        std::vector<Texture>     textures_;
        std::vector<Buffer>      buffers_;
        std::vector<Framebuffer> framebuffers_;
        std::vector<Bindings>    bindings_;
    };
} // namespace rover
