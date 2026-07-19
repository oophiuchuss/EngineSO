import GPUSceneData;

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
    float4 Albedo : SV_Target0;     // RGBA8  — albedo.rgb, alpha (AO later)
    float4 Normal : SV_Target1;     // RGBA16F — world‑space normal.xyz
    float4 MetalRough : SV_Target2; // RGBA8  — metallic, roughness
    float4 Emissive : SV_Target3;   // RGBA16F — emissive.rgb
};

// Set 1, Binding 0 — Texture heap
[[vk::binding(0, 1)]]
Sampler2D TextureHeap[];

// Set 2, Binding 1 — Material data
[[vk::binding(1, 2)]]
StructuredBuffer<MaterialData> gMaterials;

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

    // Fetch material using inde`zx passed from vertex shader
    MaterialData material = gMaterials[input.inMaterialIndex];

    // Sample albedo — multiply texture by factor (glTF spec correct)
    float4 Albedo = TextureHeap[NonUniformResourceIndex(material.AlbedoIndex)]
                        .Sample(input.inUV) * material.AlbedoColor;

    // Alpha-mask discard — only applies to AlphaMode::Mask (value 1)
    if (material.AlphaMode == 1u && Albedo.a < material.AlphaCutoff)
    {
        discard;
    }

    // Sample metallic/roughness — glTF packs: G=roughness, B=metallic
    float4 MetallicRoughness = TextureHeap[NonUniformResourceIndex(material.MetallicRoughnessIndex)]
                                    .Sample(input.inUV);
    float Metallic  = MetallicRoughness.b * material.Metallic;
    float Roughness = MetallicRoughness.g * material.Roughness;

    // Sample occlusion — R channel
    float Occlusion = TextureHeap[NonUniformResourceIndex(material.OcclusionIndex)]
                          .Sample(input.inUV).r;

    // Sample emissive
    float3 Emissive = TextureHeap[NonUniformResourceIndex(material.EmissiveIndex)]
                          .Sample(input.inUV).rgb * material.EmissiveStrength;

    uint NormalTextureIndex = NonUniformResourceIndex(material.NormalIndex);

    // Normal mapping — sample and apply via TBN, falling back cleanly to the
    // geometric vertex normal when a material has no normal texture 
    float3 SampledNormalTS = TextureHeap[NormalTextureIndex].SampleBias(input.inUV, material.NormalLodBias).rgb;

    float3 DecodedNormalTS = SampledNormalTS * 2.0 - 1.0;

    float FilteredNormalLength = length(DecodedNormalTS);

    float3 GeometricNormal = normalize(input.inNormal);

    float3 FinalNormal = ApplyNormalMap(GeometricNormal, input.inTangent, SampledNormalTS, material.NormalScale);

    // Write G‑buffer attachments
    output.Albedo       = float4(Albedo.rgb, Occlusion);                        // pack occlusion into alpha
    output.Normal       = float4(FinalNormal, saturate(FilteredNormalLength));  // world-space normal
    output.MetalRough   = float4(Metallic, Roughness, 0.0, 0.0);                // metallic, roughness
    output.Emissive     = float4(Emissive, 0.0);                                // HDR emissive

    return output;
}
