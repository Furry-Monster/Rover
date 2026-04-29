#pragma once

namespace rover
{

    class GraphicsDevice;
    class Window;
    class EventPump;
    class TimeSource;

    // Phase 2 exit-condition demo: loads / builds an ECS scene with a camera, a
    // directional light, and one cube mesh, then renders via the FrameGraph in a
    // loop. Returns the process exit code.
    int run_mesh_demo(GraphicsDevice& device, Window& window, EventPump& pump, TimeSource& time);

} // namespace rover
