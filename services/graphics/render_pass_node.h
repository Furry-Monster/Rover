#pragma once

#include "core/typedefs.h"
#include "services/graphics/render_resource.h"

#include <functional>
#include <string>
#include <vector>

namespace rover
{

    class FrameGraph;
    class GraphicsDevice;

    constexpr u32 INVALID_PASS_ID = 0xFFFFFFFFu;

    // ---------------------------------------------------------------------------
    // PassBuilder: callback handle exposed to the user during pass setup.
    //
    // Setup is a pure declaration phase: the user lists which resources the pass
    // reads / writes (so the graph can derive ordering) and stores
    // the resulting RenderResourceId values for use during execute.
    // ---------------------------------------------------------------------------
    class PassBuilder
    {
    public:
        PassBuilder(FrameGraph& graph, u32 pass_index);

        // Read the graph writer's output, or external contents if no writer.
        void read(RenderResourceId id);
        // A resource may be written by only one pass; feedback is unsupported.
        void write(RenderResourceId id);

        [[nodiscard]] FrameGraph& graph() const { return graph_; }

    private:
        FrameGraph& graph_;
        u32         pass_index_;
    };

    // Execution context passed to a pass's execute callback.
    struct PassExecuteContext
    {
        FrameGraph&     graph;
        GraphicsDevice& device;
        // Command list opened for this pass; the graph is responsible for
        // begin/end render pass when an attachment is set. The callback owns
        // viewport/scissor setup and synchronization not handled by the driver.
        u64 cmd;
    };

    using PassSetupFn   = std::function<void(PassBuilder&)>;
    using PassExecuteFn = std::function<void(PassExecuteContext&)>;

    // ---------------------------------------------------------------------------
    // RenderPassNode: declarative + executable pass record.
    // ---------------------------------------------------------------------------
    struct RenderPassNode
    {
        std::string   name;
        PassSetupFn   setup;
        PassExecuteFn execute;

        // Populated during setup.
        std::vector<RenderResourceId> reads;
        std::vector<RenderResourceId> writes;

        // An attachment contributes an implicit write during compilation, separate
        // from explicit writes so replacing it does not leave a stale dependency.
        RenderResourceId  color_attachment = INVALID_RESOURCE_ID;
        FramebufferHandle framebuffer      = INVALID_HANDLE;
        bool              has_clear        = false;
        /** When true and framebuffer includes depth, pass two clears (color + depth/stencil). */
        bool       clear_depth_stencil = false;
        ClearValue clear{};
    };

} // namespace rover
