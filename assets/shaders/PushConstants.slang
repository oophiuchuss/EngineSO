// Must stay in sync with PushConstants.ixx on CPU side

struct MaterialProperties
{
    float4 AlbedoColor;       // 16 bytes
    float  Metallic;          // 4 bytes
    float  Roughness;         // 4 bytes
    float  EmissiveStrength;  // 4 bytes
    
};

struct PushConstants
{
    float4x4 ModelMatrix;   // 64 bytes
    MaterialProperties Material;      // 32 bytes
    float  _Padding;                // 4 bytes — explicit, keeps alignment correct
};                                 // 96 bytes total