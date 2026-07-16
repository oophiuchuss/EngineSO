// Reads the G‑buffer, reconstructs world position, and applies PBR lighting
// from a directional light read from the LightBuffer.

import GPULightData;
import CameraUBO;

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

// Constnats
static const float PI = 3.14159265;

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

float3 ComputePBRLighting(
    float3    LightDir,
    float3    Normal,
    float3    Albedo,
    float     Metallic,
    float     Roughness,
    float3    ViewDir,
    float3    LightColor,
    float     LightIntensity,
    float     NdotL)
{
    // H = normalize(L + V) is undefined when the light and view directions
    // nearly oppose each other. A raw normalize there magnifies tiny per-pixel
    // differences into structured lighting artifacts. Keep H finite for the
    // NDF and compute V.H analytically from the two unit directions.
    float3 HalfVectorSum = LightDir + ViewDir;
    float HalfVectorLengthSq = dot(HalfVectorSum, HalfVectorSum);
    float HalfVectorValid = step(1.0e-6, HalfVectorLengthSq);
    float3 HalfVec = HalfVectorSum * rsqrt(max(HalfVectorLengthSq, 1.0e-6));

    float3 F0   = lerp(float3(0.04), Albedo, Metallic);
    float  NdotV = max(dot(Normal, ViewDir),  0.0);
    float  NdotH = max(dot(Normal, HalfVec), 0.0) * HalfVectorValid;
    float  VdotH = sqrt(saturate(0.5 + 0.5 * dot(LightDir, ViewDir)));

    float3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    float Alpha  = Roughness * Roughness;
    float Alpha2 = Alpha * Alpha;
    float Denom  = NdotH * NdotH * (Alpha2 - 1.0) + 1.0;
    float D      = Alpha2 / (PI * Denom * Denom);

    float K  = (Roughness + 1.0) * (Roughness + 1.0) / 8.0;
    float G1V = NdotV / (NdotV * (1.0 - K) + K);
    float G1L = NdotL / (NdotL * (1.0 - K) + K);
    float G   = G1V * G1L;

    float3 Specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    float3 KD = (1.0 - F) * (1.0 - Metallic);

    float3 Diffuse = KD * Albedo / PI;

    float3 LightScale = LightColor * LightIntensity * NdotL;

    return (Diffuse + Specular) * LightScale;
}

float3 ApplyDirectionalLight(
    GPULightData Light,
    float3       WorldPos,
    float3       Normal,
    float3       Albedo,
    float        Metallic,
    float        Roughness,
    float3       ViewDir)
{
    float3 LightDir = normalize(-Light.Direction.xyz);
    float  NdotL    = max(dot(Normal, LightDir), 0.0);
    return ComputePBRLighting(LightDir, Normal, Albedo, Metallic, Roughness,
                              ViewDir, Light.Color_Intensity.rgb,
                              Light.Color_Intensity.a, NdotL);
}

float3 ApplyPointLight(
    GPULightData Light,
    float3       WorldPos,
    float3       Normal,
    float3       Albedo,
    float        Metallic,
    float        Roughness,
    float3       ViewDir)
{
    float3 LightVec = Light.Position.xyz - WorldPos;
    float  Distance = length(LightVec);
    float3 LightDir = LightVec / Distance;

    float NdotL       = max(dot(Normal, LightDir), 0.0);
    float Attenuation = max(1.0 - (Distance / Light.Params.x), 0.0);
    Attenuation      *= Attenuation;

    return ComputePBRLighting(LightDir, Normal, Albedo, Metallic, Roughness,
                              ViewDir, Light.Color_Intensity.rgb,
                              Light.Color_Intensity.a, NdotL)
           * Attenuation;
}

float3 ApplySpotLight(
    GPULightData Light,
    float3       WorldPos,
    float3       Normal,
    float3       Albedo,
    float        Metallic,
    float        Roughness,
    float3       ViewDir)
{
    float3 LightVec = Light.Position.xyz - WorldPos;
    float  Distance = length(LightVec);
    float3 LightDir = LightVec / Distance;

    float NdotL = max(dot(Normal, LightDir), 0.0);

    // Distance attenuation
    float DistanceAtten = max(1.0 - (Distance / Light.Params.x), 0.0);
    DistanceAtten *= DistanceAtten;

    // Cone attenuation
    float CosTheta   = dot(LightDir, -Light.Direction.xyz);
    float ConeAtten  = smoothstep(Light.Params.z, Light.Params.y, CosTheta);

    return ComputePBRLighting(LightDir, Normal, Albedo, Metallic, Roughness,
                              ViewDir, Light.Color_Intensity.rgb,
                              Light.Color_Intensity.a, NdotL)
           * DistanceAtten * ConeAtten;
}

// Entry point

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;

    // Sample G‑buffer
    float4 Albedo       = GBuffer_Albedo.Sample(input.uv);
    float4 PackedNormal = GBuffer_Normal.Sample(input.uv);
    float4 MetalRough   = GBuffer_MetalRough.Sample(input.uv);
    float3 Emissive     = GBuffer_Emissive.Sample(input.uv).rgb;
    float  Depth        = Main_Depth.Sample(input.uv).r;

    float3 Normal = normalize(PackedNormal.xyz);
    float FilteredNormalLength = PackedNormal.a;

    float NormalVariance = (1.0 - FilteredNormalLength) / max(FilteredNormalLength, 1.0e-4);

    float Metallic      = MetalRough.x;
    float Roughness     = max(MetalRough.y, 0.04);
    float Occlusion     = Albedo.a;

    float BaseAlpha  = Roughness * Roughness;

    float FilteredAlpha2 = saturate(BaseAlpha * BaseAlpha + NormalVariance);

    float FilteredRoughness = sqrt(sqrt(FilteredAlpha2));

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
            LitColor += ApplyDirectionalLight(L, WorldPos, Normal, Albedo.rgb,
                                              Metallic, FilteredRoughness, ViewDir);
        }
        else if (Type == LightType_Point)
        {
            LitColor += ApplyPointLight(L, WorldPos, Normal, Albedo.rgb,
                                            Metallic, FilteredRoughness, ViewDir);
        }
        else if (Type == LightType_Spot)
        {
            LitColor += ApplySpotLight(L, WorldPos, Normal, Albedo.rgb,
                                       Metallic, FilteredRoughness, ViewDir);
        }
    }

    // Ambient term
    float3 Ambient = Albedo.rgb * 0.03 * Occlusion;

    float3 FinalColor = Ambient + LitColor + Emissive;

    output.outColor = float4(FinalColor, 1.0);

    return output;
}
