module;

#include <glm/glm.hpp>

export module GPULightData;

// Must match the Slang GPULightData struct exactly
export struct GPULightData
{
    glm::vec4 Position;            // world space (w = 1 for point/spot)
    glm::vec4 Direction;           // world space forward (w = 0)
    glm::vec4 Color_Intensity;     // RGB = color, A = intensity
    glm::vec4 Params;              // x = range, y = innerCos, z = outerCos, w = light type
};