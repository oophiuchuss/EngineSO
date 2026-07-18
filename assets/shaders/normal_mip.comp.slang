struct NormalMipPushConstants
{
    uint32_t SourceWidth;
    uint32_t SourceHeight;
    uint32_t DestinationWidth;
    uint32_t DestinationHeight;
};

[[vk::push_constant]]
NormalMipPushConstants Push;

[[vk::binding(0, 0)]]
Texture2D<float4> SourceMip;

[[vk::binding(1, 0)]]
[vk::image_format("rgba8")]
RWTexture2D<float4> DestinationMip;

static const float Weights[4] =
{
    1.0,
    3.0,
    3.0,
    1.0
};

[numthreads(8, 8, 1)]
void main(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    uint2 DestinationCoord = DispatchThreadID.xy;

    if (DestinationCoord.x >= Push.DestinationWidth ||
        DestinationCoord.y >= Push.DestinationHeight)
    {
        return;
    }

    // A destination texel covers source texels 2d and 2d + 1.
    // The four symmetric taps are therefore:
    // 2d - 1, 2d, 2d + 1, 2d + 2.
    int2 SourceBase = int2(DestinationCoord) * 2 - int2(1, 1);

    int2 MaxSourceCoord = int2(Push.SourceWidth - 1, Push.SourceHeight - 1);

    float4 FilteredValue = float4(0.0);

    for (int Y = 0; Y < 4; ++Y)
    {
        for (int X = 0; X < 4; ++X)
        {
            int2 SourceCoord = clamp(SourceBase + int2(X, Y), int2(0, 0), MaxSourceCoord);

            float Weight = Weights[X] * Weights[Y];

            FilteredValue += SourceMip.Load(int3(SourceCoord, 0)) * Weight;
        }
    }

    DestinationMip[DestinationCoord] = FilteredValue / 64.0;
}