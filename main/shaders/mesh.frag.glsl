#version 450

// Phase 2 demo mesh fragment shader. Computes a simple Lambertian + ambient
// shading term using the directional light supplied via push constants. The
// optional sampled albedo texture is mixed when bound; otherwise the albedo
// tint is used directly. Texturing is gated on a uniform so the same shader
// works for both untextured demos and the texture-loader test path.

layout(location = 0) in vec3 v_world_normal;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec4 v_albedo;
layout(location = 3) in vec4 v_light_dir;
layout(location = 4) in vec4 v_light_color;

layout(location = 0) out vec4 out_color;

void main() {
    vec3 n = normalize(v_world_normal);
    vec3 l = normalize(-v_light_dir.xyz);
    float ndotl = max(dot(n, l), 0.0);
    vec3 ambient = v_light_color.rgb * v_light_color.a;
    vec3 diffuse = v_light_color.rgb * ndotl;
    vec3 shaded  = v_albedo.rgb * (ambient + diffuse);
    out_color    = vec4(shaded, v_albedo.a);
}
