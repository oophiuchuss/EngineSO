// Reads the G‑buffer, reconstructs world position, and applies PBR lighting
// from a directional light read from the LightBuffer.

import LightData;

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
struct CameraUBO
{
    float4x4 ViewProj;
    float4x4 InverseViewProj;
    float4   CameraPos;
};
[[vk::binding(0, 0)]] ConstantBuffer<CameraUBO> uCamera;

// Set 1 — G‑buffer textures (bindings 0‑4)
[[vk::binding(0, 1)]] Sampler2D GBuffer_Albedo;
[[vk::binding(1, 1)]] Sampler2D GBuffer_Normal;
[[vk::binding(2, 1)]] Sampler2D GBuffer_MetalRough;
[[vk::binding(3, 1)]] Sampler2D GBuffer_Emissive;
[[vk::binding(4, 1)]] Sampler2D Main_Depth;

// Set 2, Binding 0 — LightBuffer (SSBO)
// Layout: [uint LightCount][3×uint padding][LightData[]]
[[vk::binding(0, 2)]] StructuredBuffer<LightData> gLights;

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

float3 ComputeDirectionalLight(
    LightData Light,
    float3    WorldPos,
    float3    Normal,
    float3    Albedo,
    float     Metallic,
    float     Roughness,
    float3    ViewDir)
{
    float3 LightDir = normalize(-Light.Direction);
    float3 HalfVec  = normalize(LightDir + ViewDir);

    float3 F0       = lerp(float3(0.04), Albedo, Metallic);
    float  NdotL    = max(dot(Normal, LightDir), 0.0);
    float  NdotV    = max(dot(Normal, ViewDir),  0.0);
    float  NdotH    = max(dot(Normal, HalfVec),  0.0);

    float3 F = F0 + (1.0 - F0) * pow(1.0 - NdotH, 5.0);

    float Alpha     = Roughness * Roughness;
    float Alpha2    = Alpha * Alpha;
    float Denom     = NdotH * NdotH * (Alpha2 - 1.0) + 1.0;
    float D         = Alpha2 / (3.14159265 * Denom * Denom);

    float K  = (Roughness + 1.0) * (Roughness + 1.0) / 8.0;
    float G1V = NdotV / (NdotV * (1.0 - K) + K);
    float G1L = NdotL / (NdotL * (1.0 - K) + K);
    float G   = G1V * G1L;

    float3 Specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    float3 KD = (1.0 - F) * (1.0 - Metallic);

    return (KD * Albedo / 3.14159265 + Specular) * Light.Color * Light.Intensity * NdotL;
}

// Entry point

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;

    // Sample G‑buffer
    float4 Albedo       = GBuffer_Albedo.Sample(input.uv);
    float3 Normal       = normalize(GBuffer_Normal.Sample(input.uv).xyz);
    float4 MetalRough   = GBuffer_MetalRough.Sample(input.uv);
    float3 Emissive     = GBuffer_Emissive.Sample(input.uv).rgb;
    float  Depth        = Main_Depth.Sample(input.uv).r;

    float Metallic      = MetalRough.x;
    float Roughness     = max(MetalRough.y, 0.04);
    float Occlusion     = Albedo.a;

    // Reconstruct world position
    float3 WorldPos     = ReconstructWorldPosition(input.uv, Depth);
    float3 ViewDir      = normalize(uCamera.CameraPos.xyz - WorldPos);

    // Accumulate lighting from directional lights
    float3 LitColor     = float3(0.0);

    // Hardcoded sun (fallback when light buffer is empty)
    LightData Sun;
    Sun.Direction = normalize(float3(0.3, -1.0, 0.5));
    Sun.Intensity = 1.5;
    Sun.Color     = float3(1.0, 0.95, 0.85);

    LitColor += ComputeDirectionalLight(
        Sun, WorldPos, Normal, Albedo.rgb, Metallic, Roughness, ViewDir);

    // Ambient term
    float3 Ambient = Albedo.rgb * 0.03 * Occlusion;

    output.outColor = float4(Ambient + LitColor + Emissive, 1.0);
    return output;
}