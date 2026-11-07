[[vk::binding(0, 0)]] Texture2D<float4> source_texture;
[[vk::binding(1, 0)]] SamplerState source_sampler;
struct Settings
{
    float exposure;
    uint tone_mapping;
    float2 padding;
};
[[vk::push_constant]] ConstantBuffer<Settings> settings;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertex_main(uint index : SV_VulkanVertexID)
{
    VertexOutput output;
    output.uv = float2((index << 1) & 2, index & 2);
    output.position = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 tone_map_main(VertexOutput input) : SV_Target0
{
    float3 color = max(source_texture.SampleLevel(source_sampler, input.uv, 0).rgb * settings.exposure, 0.0);
    color = settings.tone_mapping != 0 ? color / (1.0 + color) : saturate(color);
    return float4(color, 1.0);
}

[shader("fragment")]
float4 present_main(VertexOutput input) : SV_Target0
{
    return source_texture.SampleLevel(source_sampler, input.uv, 0);
}
