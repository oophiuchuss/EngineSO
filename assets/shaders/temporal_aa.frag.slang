[[vk::binding(0, 0)]] Sampler2D CurrentColorTexture;
[[vk::binding(1, 0)]] Sampler2D CurrentDepthTexture;
[[vk::binding(2, 0)]] Sampler2D VelocityTexture;
[[vk::binding(3, 0)]] Sampler2D HistoryColorTexture;
[[vk::binding(4, 0)]] Sampler2D HistoryDepthTexture;

struct PSInput
{
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};

struct PSOutput
{
    float4 HistoryColor : SV_Target0;
    float HistoryDepth : SV_Target1;
};

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;

    output.HistoryColor = CurrentColorTexture.Sample(input.uv);

    output.HistoryDepth = CurrentDepthTexture.Sample(input.uv).r;

    return output;
}