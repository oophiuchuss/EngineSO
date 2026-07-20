// Reads the G‑buffer, reconstructs world position, and applies PBR lighting
// from a directional light read from the LightBuffer.

import GPULightData;
import CameraUBO;
import NormalFiltering;
import DirectLighting;

struct PSInput
{
    float2 uv : TEXCOORD0;
};

struct PSOutput
{
    float4 outColor : SV_Target0;
};

// Descriptor sets

// Set 0, Binding 0 — Camera UBO
[[vk::binding(0, 0)]] ConstantBuffer<CameraUBO> uCamera;

// Set 1 — G‑buffer textures (bindings 0‑4)
[[vk::binding(0, 1)]] Sampler2D GBuffer_Albedo;
[[vk::binding(1, 1)]] Sampler2D GBuffer_Normal;
[[vk::binding(2, 1)]] Sampler2D GBuffer_MetalRough;
[[vk::binding(3, 1)]] Sampler2D GBuffer_Emissive;
[[vk::binding(4, 1)]] Sampler2D Main_Depth;

// Set 2, Binding 0 – light count header
[[vk::binding(0, 2)]]
ConstantBuffer<uint> LightCountRaw;

// Set 2, Binding 1 – LightData array
[[vk::binding(1, 2)]]
StructuredBuffer<GPULightData> gLights;

// Helpers

float3 ReconstructWorldPosition(float2 UV, float Depth)
{
    float4 ClipPos;
    ClipPos.x = UV.x * 2.0 - 1.0;
    ClipPos.y = UV.y * 2.0 - 1.0;
    ClipPos.z = Depth;
    ClipPos.w = 1.0;

    float4 WorldPos = mul(uCamera.InverseViewProj, ClipPos);
    return WorldPos.xyz / WorldPos.w;
}

// Entry point

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;

    // Sample G‑buffer
    float4 Albedo           = GBuffer_Albedo.Sample(input.uv);
    float4 PackedNormal     = GBuffer_Normal.Sample(input.uv);
    float4 MetalRough       = GBuffer_MetalRough.Sample(input.uv);
    float3 Emissive         = GBuffer_Emissive.Sample(input.uv).rgb;
    float  Depth            = Main_Depth.Sample(input.uv).r;
    float3 Normal           = normalize(PackedNormal.xyz);
    float NormalVariance    = NormalLengthToVariance(PackedNormal.a);
    float Metallic          = MetalRough.x;
    float Roughness         = max(MetalRough.y, 0.04);
    float Occlusion         = Albedo.a;

    float FilteredRoughness = ApplyNormalVarianceToRoughness(Roughness, NormalVariance);

    // Reconstruct world position
    float3 WorldPos     = ReconstructWorldPosition(input.uv, Depth);
    float3 ViewDir      = normalize(uCamera.CameraPos.xyz - WorldPos);

    // Accumulate lighting from all lights in the buffer
    float3 LitColor     = float3(0.0);
    uint Count          = LightCountRaw;

    for (uint i = 0; i < Count; ++i)
    {
        GPULightData L = gLights[i];
        uint Type = uint(L.Params.w);

        if (Type == LightType_Directional)
        {
            LitColor += ApplyDirectionalLight(L, Normal, Albedo.rgb, Metallic, FilteredRoughness, ViewDir);
        }
        else if (Type == LightType_Point)
        {
            LitColor += ApplyPointLight(L, WorldPos, Normal, Albedo.rgb, Metallic, FilteredRoughness, ViewDir);
        }
        else if (Type == LightType_Spot)
        {
            LitColor += ApplySpotLight(L, WorldPos, Normal, Albedo.rgb, Metallic, FilteredRoughness, ViewDir);
        }
    }

    // Ambient term
    float3 Ambient = Albedo.rgb * 0.03 * Occlusion;

    float3 FinalColor = Ambient + LitColor + Emissive;

    output.outColor = float4(FinalColor, 1.0);

    return output;
}
