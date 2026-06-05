import PushConstants;

struct VSInput
{
    [[vk::location(0)]] float3 inPosition;
    [[vk::location(1)]] float2 inUV;
    [[vk::location(2)]] float3 inNormal;
};

struct CameraUBO
{
    float4x4 viewProj;
    float4 cameraPos;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraUBO> uCamera;

[[vk::push_constant]]
PushConstants Push;

struct VSOutput
{
    [[vk::location(0)]] float2 outUV;
    [[vk::location(1)]] float3 outNormal;
    [[vk::location(2)]] float3 outWorldPos;

    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;

    float4 WorldPos  = mul(Push.ModelMatrix, float4(input.inPosition, 1.0));
    output.position  = mul(uCamera.viewProj, WorldPos);
    output.outUV       = input.inUV;

    // For now, pass the model‑space normal directly – the fragment shader
    // only does placeholder visualisation, and the model matrix is identity.
    output.outNormal = input.inNormal;

    output.outWorldPos = WorldPos.xyz;

    return output;
}