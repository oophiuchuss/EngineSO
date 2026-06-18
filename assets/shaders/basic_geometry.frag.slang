import GPUSceneData;

struct PSInput
{
    [[vk::location(0)]] float2 inUV;
    [[vk::location(1)]] float3 inNormal;
    [[vk::location(2)]] float3 inWorldPos;
    [[vk::location(3)]] nointerpolation uint inMaterialIndex;
};

struct PSOutput
{
    float4 outColor : SV_Target0;
};

// Set 1, Binding 0 — Texture heap
[[vk::binding(0, 1)]]
Sampler2D TextureHeap[];

// Set 2, Binding 1 — Material data
[[vk::binding(1, 2)]]
StructuredBuffer<MaterialData> gMaterials;

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;

    // Fetch material using inde`zx passed from vertex shader
    MaterialData material = gMaterials[input.inMaterialIndex];

    // Sample albedo — multiply texture by factor (glTF spec correct)
    float4 Albedo = TextureHeap[NonUniformResourceIndex(material.AlbedoIndex)]
                        .Sample(input.inUV) * material.AlbedoColor;

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

    // Simple Lambert lighting
    // TODO: placeholder until deferred pipeline is set up
    float3 Normal = normalize(input.inNormal);
    float3 LightDir = normalize(float3(1.0, 2.0, 1.0));
    float NdotL = max(dot(Normal, LightDir), 0.0);
    float Ambient = 0.2;

    float3 Color = Albedo.rgb * (Ambient + NdotL * Occlusion) + Emissive;

    output.outColor = float4(Color, Albedo.a);

    return output;
}