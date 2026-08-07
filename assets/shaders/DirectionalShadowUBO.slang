// Must match DirectionalShadowUniformData on the CPU.

struct DirectionalShadowUBO
{
    float4x4 LightViewProjection;

    // x = inverse resolution
    // y = receiver bias
    // z = shadow distance
    // w = unused
    float4 Parameters;

    // x = enabled
    // y = PCF enabled
    // z = shadow-casting GPULightData index
    // w = debug view
    uint4 Controls;
};