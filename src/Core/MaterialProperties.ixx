module;

#include <glm/glm.hpp>

export module MaterialProperties;

import TextureSlots;

export enum class AlphaMode : uint32_t
{
    Opaque = 0, Mask = 1, Blend = 2
};


export struct MaterialProperties
{
    // PBR factors
    glm::vec4   AlbedoColor = glm::vec4(1.0f);
    float       Metallic = 0.0f;
    float       Roughness = 1.0f;
    float       EmissiveStrength = 0.0f;
    float       NormalScale = 1.0f;

    // Texture heap indices (defaults point to default textures)
    int         AlbedoIndex = TextureSlots::DefaultWhite;
    int         NormalIndex = TextureSlots::DefaultNormal;
    int         MetallicRoughnessIndex = TextureSlots::DefaultWhite;
    int         OcclusionIndex = TextureSlots::DefaultWhite;
    int         EmissiveIndex = TextureSlots::DefaultBlack;

    AlphaMode   AlphaMode = AlphaMode::Opaque;
    float       AlphaCutoff = 0.5f;

    float       NormalLodBias = 0.0f;
};
