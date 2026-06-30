// Generates a triangle covering the entire viewport from SV_VertexID.

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

[shader("vertex")]
VSOutput main(uint VertexID : SV_VertexID)
{
    VSOutput output;

    static const float2 Positions[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };

    float2 pos  = Positions[VertexID];
    output.position = float4(pos, 0.0, 1.0);
    output.uv       = pos * 0.5 + 0.5;

    return output;
}