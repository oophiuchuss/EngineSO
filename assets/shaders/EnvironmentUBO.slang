// Must match EnvironmentUniformData in FrameUniform.ixx.

struct EnvironmentUBO
{
    float4x4 WorldToEnvironment;

    uint CubemapIndex;
    float Intensity;
    float2 Padding;
};