// Must stay in sync with PushConstants.ixx on CPU side

struct MaterialPushData
{
    float4 AlbedoColor;       // 16 bytes
    float  Metallic;          // 4 bytes
    float  Roughness;         // 4 bytes
    float  EmissiveStrength;  // 4 bytes
    float  _Padding;          // 4 bytes — explicit, keeps alignment correct
};

struct PushConstants
{
    float4x4         ModelMatrix;  // 64 bytes
    MaterialPushData Material;     // 32 bytes
};                                 // 96 bytes total