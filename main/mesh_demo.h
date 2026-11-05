#pragma once

#include "core/typedefs.h"

#include <string>

namespace rover
{

    class GraphicsDevice;
    class Window;
    class EventPump;
    class TimeSource;

    // Loads a persistent ECS scene and resolves its built-in mesh resources.
    // A missing scene file is initialized with a camera, light and cube. Existing
    // files are preserved. frame_limit == 0 runs until the window closes.
    int run_mesh_demo(GraphicsDevice&    device,
                      Window&            window,
                      EventPump&         pump,
                      TimeSource&        time,
                      const std::string& scene_path,
                      u32                frame_limit = 0);

} // namespace rover
