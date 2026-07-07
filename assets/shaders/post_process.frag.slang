// HDR input -> exposure -> ACES tonemap -> sRGB gamma -> dithering -> SDR output

struct PushConstants
{
    float Exposure;
    uint32_t Flags;
};

[[vk::push_constant]] PushConstants pc;

static const uint32_t PP_ToneMapping  = 1u << 0;
static const uint32_t PP_GammaCorrect = 1u << 1;
static const uint32_t PP_Dithering    = 1u << 2;

[[vk::binding(0, 0)]] Sampler2D InputTexture;

struct PSInput
{
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};

struct PSOutput
{
    float4 outColor : SV_Target0;
};

float3 ACESFilm(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return (x * (a * x + b)) /
           (x * (c * x + d) + e);
}

// Linear RGB -> sRGB
float3 LinearToSRGB(float3 color)
{
    float3 low  = color * 12.92;
    float3 high = 1.055 * pow(max(color, 0.0), 1.0 / 2.4) - 0.055;

    return lerp(
        low,
        high,
        step(0.0031308, color)
    );
}

// Simple hash noise for dithering
float DitherNoise(float2 position)
{
    return frac(
        sin(dot(position, float2(12.9898, 78.233))) * 43758.5453
    );
}

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;

    float3 color = InputTexture.Sample(input.uv).rgb;

    // Exposure
    color *= pc.Exposure;

    // HDR -> SDR compression
    if ((pc.Flags & PP_ToneMapping) != 0)
    {
        color = ACESFilm(color);
    }

    // Linear -> display space
    if ((pc.Flags & PP_GammaCorrect) != 0)
    {
        color = LinearToSRGB(color);
    }

    // Remove 8-bit quantization banding
    if ((pc.Flags & PP_Dithering) != 0)
    {
        float noise = DitherNoise(input.position.xy);

        color += (noise - 0.5) / 255.0;
    }

    // Final clamp before UNORM framebuffer
    color = saturate(color);

    output.outColor = float4(color, 1.0);

    return output;
}