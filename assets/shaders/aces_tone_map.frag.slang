// ACES filmic tone mapping – compresses HDR values to LDR.

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

    float3 hdr = InputTexture.Sample(input.uv).rgb;
    float3 tonemapped = ACESFilm(hdr);
    output.outColor = float4(tonemapped, 1.0);

    return output;
}