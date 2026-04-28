#pragma once

#include "core/graphics/graphics_types.h"

#include <vector>

namespace rover {

// ---------------------------------------------------------------------------
// Resource descriptors
// ---------------------------------------------------------------------------

struct BufferDesc {
    usize       size;
    BufferUsage usage;
    MemoryUsage memory;
    const char* debug_name = nullptr;
};

struct TextureDesc {
    u32          width;
    u32          height;
    u32          depth        = 1;
    u32          mip_levels   = 1;
    u32          array_layers = 1;
    Format       format;
    TextureType  type         = TextureType::Texture2D;
    TextureUsage usage;
    const char*  debug_name   = nullptr;
};

struct SamplerDesc {
    Filter             min_filter;
    Filter             mag_filter;
    SamplerAddressMode address_u;
    SamplerAddressMode address_v;
    SamplerAddressMode address_w;
    f32                max_anisotropy = 1.0f;
};

struct ShaderDesc {
    ShaderStage stage;
    const u8*   bytecode;
    usize       bytecode_size;
    const char* entry_point = "main";
};

// ---------------------------------------------------------------------------
// Attachment descriptors
// ---------------------------------------------------------------------------

struct ColorAttachmentDesc {
    Format      format;
    LoadOp      load_op      = LoadOp::Clear;
    StoreOp     store_op     = StoreOp::Store;
    BlendFactor src_blend    = BlendFactor::One;
    BlendFactor dst_blend    = BlendFactor::Zero;
    BlendOp     blend_op     = BlendOp::Add;
    bool        blend_enable = false;
};

struct DepthStencilAttachmentDesc {
    Format    format;
    LoadOp    load_op            = LoadOp::Clear;
    StoreOp   store_op           = StoreOp::Store;
    CompareOp compare_op         = CompareOp::Less;
    bool      depth_test_enable  = true;
    bool      depth_write_enable = true;
};

// ---------------------------------------------------------------------------
// Render pass / framebuffer
// ---------------------------------------------------------------------------

struct RenderPassDesc {
    std::vector<ColorAttachmentDesc> color_attachments;
    DepthStencilAttachmentDesc       depth_stencil;
    bool                             has_depth_stencil = false;
    const char*                      debug_name        = nullptr;
};

struct FramebufferDesc {
    RenderPassHandle             render_pass;
    std::vector<TextureHandle>   color_attachments;
    TextureHandle                depth_stencil = INVALID_HANDLE;
    u32                          width;
    u32                          height;
};

// ---------------------------------------------------------------------------
// Vertex input layout
// ---------------------------------------------------------------------------

struct VertexAttribute {
    u32    location;
    Format format;
    u32    offset;
};

struct VertexBinding {
    u32                          binding;
    u32                          stride;
    std::vector<VertexAttribute> attributes;
};

// ---------------------------------------------------------------------------
// Graphics pipeline
// ---------------------------------------------------------------------------

struct GraphicsPipelineDesc {
    ShaderHandle                 vertex_shader;
    ShaderHandle                 fragment_shader;
    RenderPassHandle             render_pass;

    PrimitiveTopology            topology   = PrimitiveTopology::TriangleList;
    CullMode                     cull_mode  = CullMode::Back;
    FrontFace                    front_face = FrontFace::CounterClockwise;

    bool                         depth_test_enable  = true;
    bool                         depth_write_enable = true;
    CompareOp                    depth_compare_op   = CompareOp::Less;

    bool                         blend_enable = false;
    BlendFactor                  src_color_blend = BlendFactor::One;
    BlendFactor                  dst_color_blend = BlendFactor::Zero;
    BlendOp                      color_blend_op  = BlendOp::Add;
    BlendFactor                  src_alpha_blend = BlendFactor::One;
    BlendFactor                  dst_alpha_blend = BlendFactor::Zero;
    BlendOp                      alpha_blend_op  = BlendOp::Add;

    std::vector<VertexBinding>   vertex_bindings;

    const char*                  debug_name = nullptr;
};

// ---------------------------------------------------------------------------
// Dynamic state helpers
// ---------------------------------------------------------------------------

struct Viewport {
    f32 x;
    f32 y;
    f32 width;
    f32 height;
    f32 min_depth = 0.0f;
    f32 max_depth = 1.0f;
};

struct Scissor {
    i32 x;
    i32 y;
    u32 width;
    u32 height;
};

struct ClearValue {
    f32 color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    f32 depth    = 1.0f;
    u8  stencil  = 0;
};

} // namespace rover
