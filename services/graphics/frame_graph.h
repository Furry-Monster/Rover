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
    // passes. Phase 2 implementation is intentionally minimal:
    //
    //   - Resources are imported (no transient allocation).
    //   - Passes execute in declaration order (no automatic reordering).
    //   - Barriers are handled per-pass via render-pass load/store ops.
    //
    // The shape of the API matches future extension: adding a topological sort
    // + barrier insertion later won't change the user-facing API.
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
        [[nodiscard]] RenderResource&       resource(RenderResourceId id);

        // ---- Pass registration ----

        // Adds a pass. `setup` is called immediately (records reads/writes);
        // `execute` is called later from `execute()`.
        u32 add_pass(std::string name, PassSetupFn setup, PassExecuteFn execute);

        // Convenience: marks the pass's color attachment + clear value. Must be
        // called from inside `setup` or before `execute()`.
        void set_color_attachment(u32 pass_index, RenderResourceId target, const ClearValue& clear);

        // ---- Compile / execute ----

        // Compile validates the declared graph (Phase 2: ensures every pass'
        // declared writes are valid resource ids). Returns false on validation
        // failure.
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

    private:
        std::vector<RenderResource> resources_;
        std::vector<RenderPassNode> passes_;
        bool                        compiled_ = false;
    };

} // namespace rover
