import PushConstants;
import GPUSceneData;
import DirectionalShadowUBO;

struct VSInput
{
    [[vk::location(0)]] float3 inPosition;
};

struct VSOutput
{
    float4 position : SV_Position;
};

[[vk::binding(0, 0)]] ConstantBuffer<DirectionalShadowUBO> uShadow;

[[vk::binding(0, 1)]] StructuredBuffer<ObjectData> gObjects;

[[vk::push_constant]]
PushConstants Push;

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;

    const ObjectData object = gObjects[Push.ObjectIndex];

    const float4 WorldPosition = mul(object.ModelMatrix, float4(input.inPosition, 1.0));

    output.position = mul(uShadow.LightViewProjection, WorldPosition);

    return output;
}