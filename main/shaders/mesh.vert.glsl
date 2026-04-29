#version 450

// Phase 2 demo mesh shader.
// Vertex layout:
//   location 0 = vec3 position
//   location 1 = vec3 normal
//   location 2 = vec2 uv
//
// Per-frame UBO (set 0 binding 0): camera view + projection.
// Per-object push constant: world transform + light direction + color.
//
// Push constants are kept compact (<= 128 bytes) so they fit on every
// driver tier without hitting the device limit.

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(set = 0, binding = 0) uniform CameraUbo {
    mat4 view;
    mat4 projection;
} camera;

layout(push_constant) uniform PushBlock {
    mat4 world;
    vec4 light_dir;        // xyz = direction, w unused
    vec4 light_color;      // rgb = color * intensity, a = ambient term
    vec4 albedo;           // rgba albedo tint
} pc;

layout(location = 0) out vec3 v_world_normal;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec4 v_albedo;
layout(location = 3) out vec4 v_light_dir;
layout(location = 4) out vec4 v_light_color;

void main() {
    vec4 world_pos = pc.world * vec4(in_position, 1.0);
    gl_Position    = camera.projection * camera.view * world_pos;

    // Approximate normal transform (ok for uniform scale).
    v_world_normal = normalize(mat3(pc.world) * in_normal);
    v_uv           = in_uv;
    v_albedo       = pc.albedo;
    v_light_dir    = pc.light_dir;
    v_light_color  = pc.light_color;
}
