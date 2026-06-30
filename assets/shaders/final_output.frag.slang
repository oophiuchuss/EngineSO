// Pure copy – writes the input texture directly to the swapchain.

[[vk::binding(0, 0)]] Sampler2D InputTexture;

struct PSInput  { float2 uv : TEXCOORD0; };
struct PSOutput { float4 outColor : SV_Target0; };

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;

    output.outColor = InputTexture.Sample(input.uv);

    return output;
}