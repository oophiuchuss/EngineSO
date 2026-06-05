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

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;

    // Simple normal visualization
    output.outColor = float4(input.inNormal * 0.5 + 0.5, 1.0);

    return output;
}