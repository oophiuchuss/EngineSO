struct VSOutput
{
    [[vk::location(0)]] float2 outUV;
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput main(uint VertexID : SV_VertexID)
{
    VSOutput output;

    float2 Positions[3] =
    {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };

    float2 Pos = Positions[VertexID];

    output.position = float4(Pos, 0.0, 1.0);
    output.outUV = Pos * 0.5 + 0.5;

    return output;
}