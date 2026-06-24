struct VSOutput
{
    [[vk::location(0)]] float2 outUV;
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput main(uint VertexID : SV_VertexID)
{
    VSOutput output;

    // Oversized triangle covering the full screen — clipped by the viewport
    // No vertex buffer needed, positions derived purely from vertex index
    float2 Positions[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };

    float2 Pos = Positions[VertexID];
    output.position = float4(Pos, 0.0, 1.0);

    // Map clip-space [-1,1] to UV [0,1], flipping Y since Vulkan's NDC Y is down
    output.outUV = Pos * 0.5 + 0.5;

    return output;
}