// Fused post-process: exposure -> ACES tonemap -> gamma correction.

struct PushConstants
{
    float Exposure;
    uint32_t Flags;
};

[[vk::push_constant]] PushConstants pc;

static const uint32_t PP_ToneMapping  = 1u << 0;
static const uint32_t PP_GammaCorrect = 1u << 1;

[[vk::binding(0, 0)]] Sampler2D InputTexture;

struct PSInput  { float2 uv : TEXCOORD0; };
struct PSOutput { float4 outColor : SV_Target0; };

float3 ACESFilm(float3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;
    float3 color = InputTexture.Sample(input.uv).rgb;

    color *= pc.Exposure;

    if ((pc.Flags & PP_ToneMapping) != 0)
    {
        color = ACESFilm(color);
    }

    if ((pc.Flags & PP_GammaCorrect) != 0)
    {
        color = pow(color, 1.0 / 2.2);
    }

    output.outColor = float4(color, 1.0);
    return output;
}