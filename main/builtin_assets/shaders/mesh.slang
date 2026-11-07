struct CameraUbo
{
    column_major float4x4 view;
    column_major float4x4 projection;
};
struct MeshPushConstants
{
    column_major float4x4 world;
    float4 light_dir;
    float4 light_color;
    float4 albedo;
};
[[vk::binding(0, 0)]] ConstantBuffer<CameraUbo> camera;
[[vk::push_constant]] ConstantBuffer<MeshPushConstants> pc;

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
};
struct VertexOutput
{
    float4 position : SV_Position;
    float3 world_normal : NORMAL0;
    float4 albedo : COLOR0;
    float4 light_dir : TEXCOORD0;
    float4 light_color : TEXCOORD1;
};

[shader("vertex")]
VertexOutput vertex_main(VertexInput input)
{
    VertexOutput output;
    float4 world_position = mul(pc.world, float4(input.position, 1.0));
    output.position = mul(camera.projection, mul(camera.view, world_position));
    output.world_normal = normalize(mul((float3x3)pc.world, input.normal));
    output.albedo = pc.albedo;
    output.light_dir = pc.light_dir;
    output.light_color = pc.light_color;
    return output;
}

[shader("fragment")]
float4 fragment_main(VertexOutput input) : SV_Target0
{
    float diffuse = max(dot(normalize(input.world_normal), normalize(-input.light_dir.xyz)), 0.0);
    float3 light = input.light_color.rgb * (input.light_color.a + diffuse);
    return float4(input.albedo.rgb * light, input.albedo.a);
}
