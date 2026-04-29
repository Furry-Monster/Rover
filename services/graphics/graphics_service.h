#pragma once

#include "core/graphics/graphics_device.h"
#include "core/typedefs.h"
#include "services/graphics/frame_graph.h"

namespace rover
{

    // ---------------------------------------------------------------------------
    // GraphicsService: process-wide singleton owning the FrameGraph and a borrowed
    // pointer to the active GraphicsDevice.
    //
    // The device pointer is supplied by `main/` once the driver is initialized.
    // The service does not own the device's lifetime -- it is bound at startup
    // and unbound at shutdown.
    // ---------------------------------------------------------------------------
    class GraphicsService
    {
    public:
        [[nodiscard]] static GraphicsService& get();

        GraphicsService(const GraphicsService&)            = delete;
        GraphicsService& operator=(const GraphicsService&) = delete;

        // ---- Lifecycle ----
        void bind_device(GraphicsDevice* device);
        void unbind_device();

        [[nodiscard]] GraphicsDevice* device() const { return device_; }

        [[nodiscard]] FrameGraph& frame_graph() { return frame_graph_; }

        // Convenience: clears + recompiles + executes the frame graph against
        // the bound device. Caller wraps with begin_frame / end_frame / present.
        void render_frame();

    private:
        GraphicsService()  = default;
        ~GraphicsService() = default;

        GraphicsDevice* device_ = nullptr;
        FrameGraph      frame_graph_;
    };

} // namespace rover
