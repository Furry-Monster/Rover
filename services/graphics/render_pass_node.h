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

    // ---------------------------------------------------------------------------
    // PassBuilder: callback handle exposed to the user during pass setup.
    //
    // Setup is a pure declaration phase: the user lists which resources the pass
    // reads / writes (so the graph can derive ordering and barriers) and stores
    // the resulting RenderResourceId values for use during execute.
    // ---------------------------------------------------------------------------
    class PassBuilder
    {
    public:
        PassBuilder(FrameGraph& graph, u32 pass_index);

        // Declare an existing imported resource as a read input.
        void read(RenderResourceId id);
        // Declare an existing imported resource as a write output.
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
        // begin/end render pass, viewport, scissor, and post-pass barriers.
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

        // The framebuffer + clear values used by execute. Phase 2 only supports
        // a single framebuffer write target per pass (the first item in `writes`
        // that has `framebuffer != INVALID_HANDLE`). Set explicitly through
        // `set_color_attachment(...)` for clarity.
        FramebufferHandle framebuffer = INVALID_HANDLE;
        bool              has_clear   = false;
        ClearValue        clear{};
    };

} // namespace rover
