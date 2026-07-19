// GPUSceneData.slang — must match GPUSceneData.ixx exactly

struct ObjectData
{
    float4x4 ModelMatrix;
    float4x4 NormalMatrix;
    uint     MaterialIndex;
    uint     _Padding0;
    uint     _Padding1;
    uint     _Padding2;
};

struct MaterialData
{
    float4  AlbedoColor;
    float   Metallic;
    float   Roughness;
    float   EmissiveStrength;
    float   NormalScale;
    uint    AlbedoIndex;
    uint    NormalIndex;
    uint    MetallicRoughnessIndex;
    uint    OcclusionIndex;
    uint    EmissiveIndex;
    uint    AlphaMode;         // 0=Opaque, 1=Mask, 2=Blend
    float   AlphaCutoff;
    float   NormalLodBias;
};
