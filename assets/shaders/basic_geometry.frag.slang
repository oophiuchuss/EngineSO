import PushConstants;

struct PSInput
{
    [[vk::location(0)]] float2 inUV;
    [[vk::location(1)]] float3 inNormal;
    [[vk::location(2)]] float3 inWorldPos;
};

struct PSOutput
{
    float4 outColor : SV_Target0;
};

[[vk::push_constant]]
PushConstants Push;

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;

    // For now visualize normals — will be replaced by G-buffer writes
    // when deferred rendering is set up
    float3 Normal = normalize(input.inNormal);
    output.outColor = float4(Normal * 0.5 + 0.5, 1.0);

    return output;
}