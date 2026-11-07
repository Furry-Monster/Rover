#pragma once

#include "core/typedefs.h"

namespace rover
{
    class GraphicsDevice;
    class Window;
    class EventPump;

    // Shader-free GPU synchronization check: render an offscreen clear, copy to
    // a readback buffer, export HostRead, then verify every pixel after GPU idle.
    // A zero frame limit defaults to eight frames.
    int run_frame_graph_demo(GraphicsDevice& device, Window& window, EventPump& pump, u32 frame_limit);
} // namespace rover
