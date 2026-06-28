// GPULightData.slang — must match GPULightData.ixx exactly

static const uint LightType_Directional = 0;
static const uint LightType_Point       = 1;
static const uint LightType_Spot        = 2;

struct GPULightData
{
    float4 Position;
    float4 Direction;
    float4 Color_Intensity;
    float4 Params;
};