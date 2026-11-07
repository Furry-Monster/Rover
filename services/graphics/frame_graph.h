#pragma once

#include "core/graphics/graphics_device.h"
#include "core/typedefs.h"
#include "services/graphics/render_pass_node.h"
#include "services/graphics/render_resource.h"

#include <string>
#include <vector>

namespace rover
{

    // ---------------------------------------------------------------------------
    // FrameGraph: declarative render graph for organizing one frame's worth of
    // passes. Current compilation contract:
    //
    //   - Resources are imported (no transient allocation).
    //   - Each resource has at most one writer; reads depend on that writer.
    //   - Independent ready passes use registration order as a tie breaker.
    //   - No automatic GPU barriers or layout transitions are generated.
    // Graph authoring/execution is single-threaded; execution forbids mutation.
    // ---------------------------------------------------------------------------
    class FrameGraph
    {
    public:
        FrameGraph()  = default;
        ~FrameGraph() = default;

        FrameGraph(const FrameGraph&)            = delete;
        FrameGraph& operator=(const FrameGraph&) = delete;

        // ---- Resource registration ----

        // Imports an existing GPU texture into the graph. Returns a stable id
        // usable across passes within this frame.
        RenderResourceId import_texture(std::string       name,
                                        TextureHandle     handle,
                                        Format            format,
                                        u32               width,
                                        u32               height,
                                        FramebufferHandle fb = INVALID_HANDLE);

        // Imports an existing GPU buffer into the graph.
        RenderResourceId import_buffer(std::string name, BufferHandle handle);

        [[nodiscard]] const RenderResource& resource(RenderResourceId id) const;

        // ---- Pass registration ----

        // Adds a pass. `setup` is called immediately (records reads/writes);
        // `execute` is called later from `execute()`.
        u32 add_pass(std::string name, PassSetupFn setup, PassExecuteFn execute);

        // Declares an implicit write and sets the color attachment/clear value.
        // The target must be a texture with a framebuffer. Call before compile().
        void set_color_attachment(u32               pass_index,
                                  RenderResourceId  target,
                                  const ClearValue& clear,
                                  bool              clear_depth_stencil = false);

        // ---- Compile / execute ----

        // Validates references, single-writer access and acyclicity, then builds
        // a deterministic topological execution plan. Failure clears the plan.
        // Any subsequent graph mutation requires compilation again.
        bool compile();

        // Records all passes onto the device. Each pass gets its own command
        // list. Caller is responsible for calling `device.begin_frame()` before
        // and `device.end_frame() / present()` after.
        void execute(GraphicsDevice& device);

        // Discards all per-frame state (resources + passes). Call between frames.
        void reset();

        [[nodiscard]] usize pass_count() const { return passes_.size(); }

        [[nodiscard]] usize resource_count() const { return resources_.size(); }

        [[nodiscard]] const std::vector<RenderPassNode>& passes() const { return passes_; }

        [[nodiscard]] const std::vector<u32>& execution_order() const { return execution_order_; }

        // Prerequisite pass IDs indexed by registration ID; empty without a plan.
        [[nodiscard]] const std::vector<std::vector<u32>>& dependencies() const { return dependencies_; }

    private:
        friend class PassBuilder;

        bool prepare_mutation();
        void invalidate();
        void declare_access(u32 pass_index, RenderResourceId id, bool write);

        std::vector<RenderResource>   resources_;
        std::vector<RenderPassNode>   passes_;
        std::vector<u32>              execution_order_;
        std::vector<std::vector<u32>> dependencies_;
        bool                          compiled_          = false;
        bool                          executing_         = false;
        bool                          declaration_error_ = false;
    };

} // namespace rover
