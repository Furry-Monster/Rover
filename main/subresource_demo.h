#pragma once
#include "core/typedefs.h"

namespace rover
{
    class GraphicsDevice;
    class Window;
    class EventPump;
    int run_subresource_demo(GraphicsDevice& device, Window& window, EventPump& pump, u32 frame_limit);
} // namespace rover
