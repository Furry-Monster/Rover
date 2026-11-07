#pragma once

#include "core/typedefs.h"

#include <string>

namespace rover
{

    class GraphicsDevice;
    class Window;
    class EventPump;
    class TimeSource;

    struct MeshDemoOptions
    {
        f32         exposure         = 1.0f;
        bool        tone_mapping     = true;
        bool        verify_rendering = false;
        bool        resize_test      = false;
        std::string capture_path;
    };

    // Loads a persistent ECS scene and resolves its built-in mesh resources.
    // A missing scene file is initialized with a camera, light and cube. Existing
    // files are preserved. frame_limit == 0 runs until the window closes.
    int run_mesh_demo(GraphicsDevice&        device,
                      Window&                window,
                      EventPump&             pump,
                      TimeSource&            time,
                      const std::string&     scene_path,
                      u32                    frame_limit = 0,
                      const MeshDemoOptions& options     = {});

} // namespace rover
