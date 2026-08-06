module;

#include <cstdint>

#include <glm/glm.hpp>

export module DirectionalShadowSettings;

export enum class DirectionalShadowDebugView : uint32_t
{
    None = 0,
    Depth = 1,
    Visibility = 2
};

export struct DirectionalShadowSettings
{
    bool bEnabled = true;
    bool bPCFEnabled = true;

    uint32_t Resolution = 2048;

    // Maximum camera-relative distance covered by the single shadow map.
    float ShadowDistance = 50.0f;

    // Additional depth space for casters located behind the fitted receiver area.
    float DepthPadding = 20.0f;

    // Vulkan rasterization bias.
    float RasterConstantBias = 1.25f;
    float RasterSlopeBias = 1.75f;

    // Small comparison bias applied while sampling the shadow map.
    float ReceiverBias = 0.0005f;

    DirectionalShadowDebugView DebugView = DirectionalShadowDebugView::None;
};

// Uploaded once per frame.
//
// Controls:
// x = shadow enabled
// y = PCF enabled
// z = index of the shadow-casting light in GPULightData[]
// w = DirectionalShadowDebugView
//
// Parameters:
// x = inverse shadow-map resolution
// y = receiver comparison bias
// z = shadow distance
// w = unused
export struct alignas(16) DirectionalShadowUniformData
{
    glm::mat4 LightViewProjection = glm::mat4(1.0f);

    glm::vec4 Parameters = glm::vec4(0.0f);

    glm::uvec4 Controls = glm::uvec4(0);
};