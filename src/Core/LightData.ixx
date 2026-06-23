module;

#include <glm/glm.hpp>

export module LightData;

// Must match the Slang LightData struct exactly — same field order, same sizes.
// Total size: 32 bytes (std430 padded), so no cross‑field alignment issues.
export struct LightData
{
    glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f);   // normalised, directional lights only for now
    float Intensity = 1.0f;
    glm::vec3 Color = glm::vec3(1.0f);
    float _Padding = 0.0f;   // keep 16‑byte alignment for std430
};