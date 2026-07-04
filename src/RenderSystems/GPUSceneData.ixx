module;

#include <glm/glm.hpp>

export module GPUSceneData;

// Per-object data — indexed by ObjectIndex from push constants
// NormalMatrix uses mat4 (not mat3) to avoid GLM/GPU std430 mat3 alignment mismatch
// (CPU mat3 = 36 bytes, GPU std430 mat3 = 48 bytes due to column padding to vec4)
export struct ObjectData
{
    glm::mat4 ModelMatrix = glm::mat4(1.0f);
    glm::mat4 NormalMatrix = glm::mat4(1.0f);   // only upper-left 3x3 used by shader
    uint32_t MaterialIndex = 0;
    uint32_t _Padding[3] = { 0, 0, 0 };         // align struct to 16 bytes
};
// Size: 64 + 64 + 4 + 12 = 144 bytes

// Per-material data — indexed by ObjectData.MaterialIndex
export struct MaterialData
{
    glm::vec4 AlbedoColor = glm::vec4(1.0f);
    float Metallic = 0.0f;
    float Roughness = 1.0f;
    float EmissiveStrength = 0.0f;
    uint32_t AlbedoIndex = 0;
    uint32_t NormalIndex = 0;
    uint32_t MetallicRoughnessIndex = 0;
    uint32_t OcclusionIndex = 0;
    uint32_t EmissiveIndex = 0;
    uint32_t AlphaMode = 0;
    float AlphaCutoff = 0.5f;
    uint32_t _Padding[2]; // pad 56 -> 64 bytes: std430 rounds struct-array stride 
};
// Size: 56 + 8 = 64 bytes — matches std430 array stride exactly