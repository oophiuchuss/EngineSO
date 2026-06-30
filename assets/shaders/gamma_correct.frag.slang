// Applies gamma correction (linear → sRGB) for UNORM swapchains.

[[vk::binding(0, 0)]] Sampler2D InputTexture;

struct PSInput  { float2 uv : TEXCOORD0; };
struct PSOutput { float4 outColor : SV_Target0; };

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;

    float4 color = InputTexture.Sample(input.uv);
    float3 corrected = pow(color.rgb, 1.0 / 2.2);
    output.outColor = float4(corrected, color.a);

    return output;
}