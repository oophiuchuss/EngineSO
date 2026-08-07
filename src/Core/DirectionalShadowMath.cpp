module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

module DirectionalShadowMath;

DirectionalShadowUniformData BuildDirectionalShadowUniformData(
    const glm::vec3& CameraPosition,
    const glm::vec3& CameraForward,
    float CameraVerticalFieldOfViewDegrees,
    float CameraAspectRatio,
    float CameraNearPlane,
    const glm::vec3& LightDirection,
    uint32_t ShadowLightIndex,
    const DirectionalShadowSettings& Settings)
{
    if (Settings.Resolution == 0)
    {
        throw std::invalid_argument("Directional shadow resolution must be greater than zero");
    }

    if (Settings.ShadowDistance <= CameraNearPlane)
    {
        throw std::invalid_argument("Directional shadow distance must be greater than the camera near plane");
    }

    if (CameraAspectRatio <= 0.0f)
    {
        throw std::invalid_argument("Directional shadow calculation requires a positive camera aspect ratio");
    }

    const float CameraForwardLengthSquared = glm::dot(CameraForward, CameraForward);

    const float LightDirectionLengthSquared = glm::dot(LightDirection, LightDirection);

    if (CameraForwardLengthSquared <= 1.0e-8f || LightDirectionLengthSquared <= 1.0e-8f)
    {
        throw std::invalid_argument("Directional shadow calculation received a zero-length direction");
    }

    const glm::vec3 NormalizedCameraForward = glm::normalize(CameraForward);

    // Direction in which light rays travel: light -> scene.
    const glm::vec3 NormalizedLightDirection = glm::normalize(LightDirection);

    const float NearDistance = (std::max)(CameraNearPlane, 0.001f);

    const float FarDistance = Settings.ShadowDistance;

    const float HalfDepth = (FarDistance - NearDistance) * 0.5f;

    const float CenterDistance = (NearDistance + FarDistance) * 0.5f;

    const float TangentHalfVerticalFov = std::tan(glm::radians(CameraVerticalFieldOfViewDegrees) * 0.5f);

    const float FarHalfHeight = FarDistance * TangentHalfVerticalFov;

    const float FarHalfWidth = FarHalfHeight * CameraAspectRatio;

    // Conservative sphere covering the camera frustum slice.
    //
    // Keeping this radius dependent only on camera projection settings and
    // shadow distance prevents its size from changing as the camera moves.
    const float ShadowRadius = std::sqrt(HalfDepth * HalfDepth + FarHalfWidth * FarHalfWidth + FarHalfHeight * FarHalfHeight);

    const glm::vec3 FrustumCenter = CameraPosition + NormalizedCameraForward * CenterDistance;

    glm::vec3 LightUp(0.0f, 1.0f, 0.0f);

    // Avoid an unstable lookAt basis when the light points almost exactly
    // along the normal world-up axis.
    if (std::abs(glm::dot(NormalizedLightDirection, LightUp)) > 0.99f)
    {
        LightUp = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    // This view contains only the light's orientation because its eye is at
    // the origin. It lets us express the projection center in light space.
    const glm::mat4 LightOrientation = glm::lookAt(glm::vec3(0.0f), NormalizedLightDirection, LightUp);

    glm::vec3 CenterLightSpace = glm::vec3(LightOrientation * glm::vec4(FrustumCenter, 1.0f));

    const float WorldUnitsPerShadowTexel = (ShadowRadius * 2.0f) / static_cast<float>(Settings.Resolution);

    // Quantize movement perpendicular to the light direction. Sub-texel
    // camera movement therefore does not slide geometry across the map.
    CenterLightSpace.x = std::round(CenterLightSpace.x / WorldUnitsPerShadowTexel) * WorldUnitsPerShadowTexel;
    CenterLightSpace.y = std::round(CenterLightSpace.y / WorldUnitsPerShadowTexel) * WorldUnitsPerShadowTexel;

    const glm::mat4 InverseLightOrientation = glm::inverse(LightOrientation);

    const glm::vec3 SnappedFrustumCenter = glm::vec3(InverseLightOrientation * glm::vec4(CenterLightSpace, 1.0f));

    const float DepthPadding = (std::max)(Settings.DepthPadding, 0.0f);

    const float EyeDistance = ShadowRadius + DepthPadding;

    const glm::vec3 LightEye = SnappedFrustumCenter - NormalizedLightDirection * EyeDistance;

    const glm::mat4 LightView = glm::lookAt(LightEye, SnappedFrustumCenter, LightUp);

    const float ShadowFarPlane = 2.0f * (ShadowRadius + DepthPadding);

    glm::mat4 LightProjection = glm::ortho(-ShadowRadius, ShadowRadius, -ShadowRadius, ShadowRadius, 0.0f, ShadowFarPlane);

    // Match the Vulkan projection convention already used by CameraComponent.
    LightProjection[1][1] *= -1.0f;

    DirectionalShadowUniformData Result;

    Result.LightViewProjection = LightProjection * LightView;

    Result.Parameters = glm::vec4(1.0f / static_cast<float>(Settings.Resolution), Settings.ReceiverBias, Settings.ShadowDistance, 0.0f);

    Result.Controls = glm::uvec4(1u, Settings.bPCFEnabled ? 1u : 0u, ShadowLightIndex, static_cast<uint32_t>(Settings.DebugView));

    return Result;
}