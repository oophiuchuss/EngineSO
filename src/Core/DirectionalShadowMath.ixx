module;

#include <cstdint>

#include <glm/glm.hpp>

export module DirectionalShadowMath;

import DirectionalShadowSettings;

export DirectionalShadowUniformData BuildDirectionalShadowUniformData(
    const glm::vec3& CameraPosition,
    const glm::vec3& CameraForward,
    float CameraVerticalFieldOfViewDegrees,
    float CameraAspectRatio,
    float CameraNearPlane,
    const glm::vec3& LightDirection,
    uint32_t ShadowLightIndex,
    const DirectionalShadowSettings& Settings);