import GPUSceneData;
import GPULightData;
import CameraUBO;
import NormalFiltering;
import DirectLighting;

struct PSInput
{
    [[vk::location(0)]] float2 inUV;
    [[vk::location(1)]] float3 inNormal;
    [[vk::location(2)]] float3 inWorldPos;
    [[vk::location(3)]] nointerpolation uint inMaterialIndex;
    [[vk::location(4)]] float4 inTangent; // world-space xyz, w = handedness sign
};

struct PSOutput
{
    float4 outColor : SV_Target0;
};

// Set 0, Binding 0 — Camera UBO
[[vk::binding(0, 0)]] ConstantBuffer<CameraUBO> uCamera;

// Set 1 — Texture heap
[[vk::binding(0, 1)]] Sampler2D TextureHeap[];

// Set 2 — GPU scene data
[[vk::binding(1, 2)]] StructuredBuffer<MaterialData> gMaterials;

// Set 3 — Light buffer (mirrors LightingPass's set 2, shifted to set 3 here
// since sets 1/2 are already taken by DescriptorHeap/GPUSceneBuffer)
[[vk::binding(0, 3)]] ConstantBuffer<uint> LightCountRaw;
[[vk::binding(1, 3)]] StructuredBuffer<GPULightData> gLights;

float3 ApplyNormalMap(
    float3 GeoNormal,
    float4 Tangent,
    float3 SampledNormalTS,
    float NormalScale)
{
    // Reconstruct orthonormal TBN. Re-orthogonalize tangent against normal
    // (Gram-Schmidt) since interpolation across a triangle can leave them
    // not-quite-perpendicular even if they started that way per-vertex.
    float3 N = normalize(GeoNormal);
    float3 T = normalize(Tangent.xyz - N * dot(N, Tangent.xyz));
    float3 B = cross(N, T) * Tangent.w; // handedness sign flips B for mirrored UVs

    float3x3 TBN = float3x3(T, B, N);

    // Normal-map RGB is [0,1]-encoded; remap it to a signed tangent-space vector.
    float3 TangentSpaceNormal = SampledNormalTS * 2.0 - 1.0;

    TangentSpaceNormal.xy *= NormalScale;
    float ScaledLength = length(TangentSpaceNormal);
    TangentSpaceNormal = ScaledLength > 0.0001
        ? TangentSpaceNormal / ScaledLength
        : float3(0.0, 0.0, 1.0);

    return normalize(mul(TangentSpaceNormal, TBN));
}

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;

    MaterialData material = gMaterials[input.inMaterialIndex];

    float4 Albedo = TextureHeap[NonUniformResourceIndex(material.AlbedoIndex)].Sample(input.inUV) * material.AlbedoColor;
    float4 MetallicRoughness = TextureHeap[NonUniformResourceIndex(material.MetallicRoughnessIndex)].Sample(input.inUV);
    float Metallic = MetallicRoughness.b * material.Metallic;
    float Roughness = max(MetallicRoughness.g * material.Roughness, 0.04);
    float Occlusion = TextureHeap[NonUniformResourceIndex(material.OcclusionIndex)].Sample(input.inUV).r;
    float3 Emissive = TextureHeap[NonUniformResourceIndex(material.EmissiveIndex)].Sample(input.inUV).rgb * material.EmissiveStrength;

    // Normal mapping — sample and apply via TBN, falling back cleanly to the
    // geometric vertex normal when a material has no normal texture 
    float3 SampledNormalTS = TextureHeap[NonUniformResourceIndex(material.NormalIndex)].SampleBias(input.inUV, material.NormalLodBias).rgb;

    float3 DecodedNormalTS = SampledNormalTS * 2.0 - 1.0;

    float NormalVariance = NormalLengthToVariance(length(DecodedNormalTS));
    NormalVariance = ScaleNormalVariance(NormalVariance, material.NormalScale);
    Roughness = ApplyNormalVarianceToRoughness(Roughness, NormalVariance);

    float3 Normal = ApplyNormalMap(normalize(input.inNormal), input.inTangent, SampledNormalTS, material.NormalScale);

    float3 ViewDir = normalize(uCamera.CameraPos.xyz - input.inWorldPos);

    float3 LitColor = float3(0.0);
    uint Count = LightCountRaw;
    for (uint i = 0; i < Count; ++i)
    {
        GPULightData L = gLights[i];
        uint Type = uint(L.Params.w);
        if (Type == LightType_Directional)
        {
            LitColor += ApplyDirectionalLight(L, Normal, Albedo.rgb, Metallic, Roughness, ViewDir);
        }
        else if (Type == LightType_Point)
        {
            LitColor += ApplyPointLight(L, input.inWorldPos, Normal, Albedo.rgb, Metallic, Roughness, ViewDir);
        }
        else if (Type == LightType_Spot)
        {
            LitColor += ApplySpotLight(L, input.inWorldPos, Normal, Albedo.rgb, Metallic, Roughness, ViewDir);
        }
    }

    float3 Ambient = Albedo.rgb * 0.03 * Occlusion;
    float3 FinalColor = LitColor + Emissive + Ambient;

    // Alpha drives the fixed-function blend (SrcAlpha/OneMinusSrcAlpha, set in PipelineCache)
    output.outColor = float4(FinalColor, Albedo.a);
    return output;
}
