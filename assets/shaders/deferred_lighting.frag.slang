// Reads the G-buffer, renders untouched depth as the environment,
// and applies PBR lighting to visible geometry.

import GPULightData;
import CameraUBO;
import EnvironmentUBO;
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

// Set 0: frame-global uniforms.
[[vk::binding(0, 0)]] ConstantBuffer<CameraUBO> uCamera;
[[vk::binding(1, 0)]] ConstantBuffer<EnvironmentUBO> uEnvironment;

// Set 1 — G‑buffer textures (bindings 0‑4)
[[vk::binding(0, 1)]] Sampler2D GBuffer_Albedo;
[[vk::binding(1, 1)]] Sampler2D GBuffer_Normal;
[[vk::binding(2, 1)]] Sampler2D GBuffer_MetalRough;
[[vk::binding(3, 1)]] Sampler2D GBuffer_Emissive;
[[vk::binding(4, 1)]] Sampler2D GBuffer_Velocity;
[[vk::binding(5, 1)]] Sampler2D Main_Depth;

// Set 2, Binding 0 – light count header
[[vk::binding(0, 2)]] ConstantBuffer<uint> LightCountRaw;

// Set 2, Binding 1 – LightData array
[[vk::binding(1, 2)]] StructuredBuffer<GPULightData> gLights;

// Set 3: descriptor heap.
// Binding 0 contains Sampler2D[], but this shader does not use it.
[[vk::binding(1, 3)]] SamplerCube CubemapHeap[];

// Push constants
struct LightingPushConstants
{
    uint  DebugView;
    float MotionVectorScale;
};

[[vk::push_constant]]
LightingPushConstants Push;

// Debug view modes
static const uint RenderDebugView_None = 0;
static const uint RenderDebugView_MotionVectors = 1;

// Helpers
float3 ReconstructWorldPosition(float2 UV, float Depth)
{
    float4 ClipPosition;

    ClipPosition.x = UV.x * 2.0 - 1.0;
    ClipPosition.y = UV.y * 2.0 - 1.0;
    ClipPosition.z = Depth;
    ClipPosition.w = 1.0;

    float4 WorldPosition = mul(uCamera.InverseViewProj, ClipPosition);
    return WorldPosition.xyz / WorldPosition.w;
}

float3 ReconstructWorldDirection(float2 UV)
{
    float3 FarWorldPosition = ReconstructWorldPosition(UV, 1.0);

    return normalize(FarWorldPosition - uCamera.CameraPos.xyz);
}

float3 SampleEnvironment(float3 WorldDirection)
{
    float3 EnvironmentDirection = mul(uEnvironment.WorldToEnvironment, float4(WorldDirection, 0.0)).xyz;

    EnvironmentDirection = normalize(EnvironmentDirection);

    uint CubemapIndex = NonUniformResourceIndex(uEnvironment.CubemapIndex);

    float3 EnvironmentColor = CubemapHeap[CubemapIndex].Sample(EnvironmentDirection).rgb;

    return EnvironmentColor * uEnvironment.Intensity;
}

// Entry point

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;

    if (Push.DebugView == RenderDebugView_MotionVectors)
    {
        float2 Velocity = GBuffer_Velocity.Sample(input.uv).xy;
        float2 EncodedVelocity = saturate(Velocity * Push.MotionVectorScale + 0.5);
        output.outColor = float4(EncodedVelocity.x, EncodedVelocity.y, 0.5, 1.0);

        return output;
    }

    // Depth is sampled first because it determines whether this pixel
    // represents geometry or untouched background.
    float Depth = Main_Depth.Sample(input.uv).r;

    if (Depth >= 1.0)
    {
        float3 WorldDirection = ReconstructWorldDirection(input.uv);
        float3 SkyColor = SampleEnvironment(WorldDirection);

        output.outColor = float4(SkyColor, 1.0);

        return output;
    }

    // Only read and process G-buffer surface data when geometry exists.
    float4  Albedo          = GBuffer_Albedo.Sample(input.uv);
    float4  PackedNormal    = GBuffer_Normal.Sample(input.uv);
    float4  MetalRough      = GBuffer_MetalRough.Sample(input.uv);
    float3  Emissive        = GBuffer_Emissive.Sample(input.uv).rgb;
    float3  Normal          = normalize(PackedNormal.xyz);
    float   NormalVariance  = NormalLengthToVariance(PackedNormal.a);
    float   Metallic        = MetalRough.x;
    float   Roughness       = max(MetalRough.y, 0.04);
    float   Occlusion       = Albedo.a;

    float FilteredRoughness = ApplyNormalVarianceToRoughness(Roughness, NormalVariance);

    float3 WorldPosition = ReconstructWorldPosition(input.uv, Depth);
    float3 ViewDirection = normalize(uCamera.CameraPos.xyz - WorldPosition);

    float3 LitColor = float3(0.0);
    uint LightCount = LightCountRaw;

    for (uint LightIndex = 0; LightIndex < LightCount; LightIndex++)
    {
        GPULightData Light = gLights[LightIndex];
        uint Type = uint(Light.Params.w);

        if (Type == LightType_Directional)
        {
            LitColor += ApplyDirectionalLight(Light, Normal, Albedo.rgb, Metallic, FilteredRoughness, ViewDirection);
        }
        else if (Type == LightType_Point)
        {
            LitColor += ApplyPointLight(Light, WorldPosition, Normal, Albedo.rgb, Metallic, FilteredRoughness, ViewDirection);
        }
        else if (Type == LightType_Spot)
        {
            LitColor += ApplySpotLight(Light, WorldPosition, Normal, Albedo.rgb, Metallic, FilteredRoughness, ViewDirection);
        }
    }

    // Ambient term
    float3 Ambient = Albedo.rgb * 0.03 * Occlusion;

    float3 FinalColor = Ambient + LitColor + Emissive;

    output.outColor = float4(FinalColor, 1.0);

    return output;
}