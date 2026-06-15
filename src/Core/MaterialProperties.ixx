module;

#include <glm/glm.hpp>

export module MaterialProperties;

import TextureSlots;

// Must match PushConstants.slang exactly — same field order, same sizes
// Total size: 64 (model mat) + 16 + 4 + 4 + 4 + (5*4) = 112 bytes
// Vulkan guarantees minimum 128 bytes for push constants

export struct MaterialProperties
{
    // PBR factors
    glm::vec4 AlbedoColor = glm::vec4(1.0f);
    float     Metallic = 0.0f;
    float     Roughness = 1.0f;
    float     EmissiveStrength = 0.0f;

    // Texture heap indices (defaults point to default textures)
    int AlbedoIndex = TextureSlots::DefaultWhite;
    int NormalIndex = TextureSlots::DefaultNormal;
    int MetallicRoughnessIndex = TextureSlots::DefaultWhite;
    int OcclusionIndex = TextureSlots::DefaultWhite;
    int EmissiveIndex = TextureSlots::DefaultBlack;
};