#pragma once

#include "core/graphics/graphics_device.h"
#include "core/typedefs.h"
#include "services/graphics/render_pass_node.h"
#include "services/graphics/render_resource.h"

#include <string>
#include <vector>

namespace rover
{
    class TransientResourcePool;

    // ---------------------------------------------------------------------------
    // FrameGraph: declarative render graph for organizing one frame's worth of
    // passes. Current compilation contract:
    //
    //   - Resources are imported (no transient allocation).
    //   - Each version has at most one writer; physical overwrite chains add WAR/WAW.
    //   - Independent ready passes use registration order as a tie breaker.
    //   - Resource usages compile into barriers; drivers track physical state.
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

        RenderResourceId  create_texture(std::string name, const TextureDesc& desc);
        RenderResourceId  create_buffer(std::string name, const BufferDesc& desc);
        RenderBindGroupId create_bind_group(BindGroupLayoutHandle layout, std::vector<RenderBinding> entries);
        [[nodiscard]] BindGroupHandle bind_group(RenderBindGroupId id) const;

        // Creates the next logical version of the same physical resource.
        // Chains are linear; old readers finish before the successor's writer.
        // Each successor needs a writer, and one pass uses only one version.
        RenderResourceId create_version(RenderResourceId previous, std::string name = {});

        [[nodiscard]] const RenderResource& resource(RenderResourceId id) const;

        // Export the last version for a subsequent GPU use or CPU readback. HostRead
        // additionally requires GPU completion before mapping on the CPU.
        void set_final_access(RenderResourceId id, ResourceAccess access);

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

        // The pool creates/caches this framebuffer after resources are resolved.
        void set_render_target(u32               pass_index,
                               RenderPassHandle  render_pass,
                               RenderResourceId  color,
                               const ClearValue& clear,
                               RenderResourceId  depth = INVALID_RESOURCE_ID);

        // ---- Compile / execute ----

        // Validates references, version chains and acyclicity, then builds
        // a deterministic topological execution plan. Failure clears the plan.
        // Any subsequent graph mutation requires compilation again.
        bool compile();
        bool prepare_resources(TransientResourcePool& pool);

        // Records all passes onto the device. Each pass gets its own command
        // list. Caller is responsible for calling `device.begin_frame()` before
        // and `device.end_frame() / present()` after.
        // False stops recording downstream passes/exports; submitted work is not
        // rolled back. Callbacks must handle their own command failures.
        bool execute(GraphicsDevice& device);

        // Discards all per-frame state (resources + passes). Call between frames.
        void reset();

        [[nodiscard]] usize pass_count() const { return passes_.size(); }

        [[nodiscard]] usize resource_count() const { return resources_.size(); }

        [[nodiscard]] const std::vector<RenderPassNode>& passes() const { return passes_; }

        [[nodiscard]] const std::vector<u32>& execution_order() const { return execution_order_; }

        // Prerequisite pass IDs indexed by registration ID; empty without a plan.
        [[nodiscard]] const std::vector<std::vector<u32>>& dependencies() const { return dependencies_; }

        [[nodiscard]] const std::vector<std::vector<RenderResourceBarrier>>& barriers() const { return barriers_; }

        [[nodiscard]] const std::vector<RenderResourceBarrier>& final_barriers() const { return final_barriers_; }

    private:
        friend class PassBuilder;
        friend class TransientResourcePool;

        bool prepare_mutation();
        void invalidate();
        void declare_access(u32 pass_index, RenderResourceId id, ResourceAccess access, bool write);
        bool record_barriers(GraphicsDevice&                           device,
                             CommandListHandle                         cmd,
                             const std::vector<RenderResourceBarrier>& barriers) const;

        std::vector<RenderResource>                     resources_;
        std::vector<RenderPassNode>                     passes_;
        std::vector<RenderBindGroup>                    bind_groups_;
        std::vector<u32>                                execution_order_;
        std::vector<std::vector<u32>>                   dependencies_;
        std::vector<std::vector<RenderResourceBarrier>> barriers_;
        std::vector<RenderResourceBarrier>              final_barriers_;
        bool                                            compiled_          = false;
        bool                                            executing_         = false;
        bool                                            declaration_error_ = false;
        u64                                             prepared_serial_   = 0;
        TransientResourcePool*                          prepared_pool_     = nullptr;
        u64                                             prepared_epoch_    = 0;
        GraphicsDevice*                                 prepared_device_   = nullptr;
    };

} // namespace rover
