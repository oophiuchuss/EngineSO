import PushConstants;
import GPUSceneData;

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

// Set 0, Binding 0 — Camera UBO
[[vk::binding(0, 0)]]
ConstantBuffer<CameraUBO> uCamera;

// Set 1 is texture heap, it is not used in vertex shader

// Set 2, Binding 0 — Object data
[[vk::binding(0, 2)]]
StructuredBuffer<ObjectData> gObjects;

// Set 2, Binding 1 — Material data (only MaterialIndex needed here)
[[vk::binding(1, 2)]]
StructuredBuffer<MaterialData> gMaterials;

// Push constants — just the object index
[[vk::push_constant]]
PushConstants Push;

struct VSOutput
{
    [[vk::location(0)]] float2 outUV;
    [[vk::location(1)]] float3 outNormal;
    [[vk::location(2)]] float3 outWorldPos;
    [[vk::location(3)]] nointerpolation uint outMaterialIndex;

    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;

    // Fetch this object's transform data
    ObjectData object = gObjects[Push.ObjectIndex];

    // Transform position to world space then clip space
    float4 WorldPos = mul(object.ModelMatrix, float4(input.inPosition, 1.0));
    output.position = mul(uCamera.viewProj, WorldPos);
    output.outUV = input.inUV;
    output.outWorldPos = WorldPos.xyz;

    // Transform normal using precomputed normal matrix (inverse transpose of model)
    // Only upper-left 3x3 of NormalMatrix is used — avoids mat3 alignment issues on CPU
    float3x3 NormalMat = (float3x3)object.NormalMatrix;
    output.outNormal = normalize(mul(NormalMat, input.inNormal));

    // Pass material index to fragment shader
    output.outMaterialIndex = object.MaterialIndex;

    return output;
}