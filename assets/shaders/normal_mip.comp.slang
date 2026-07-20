struct NormalMipPushConstants
{
    uint32_t SourceWidth;
    uint32_t SourceHeight;
    uint32_t DestinationWidth;
    uint32_t DestinationHeight;
    uint32_t AddressModeU;
    uint32_t AddressModeV;
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

static const uint ADDRESS_REPEAT = 0;
static const uint ADDRESS_CLAMP  = 1;
static const uint ADDRESS_MIRROR = 2;

int PositiveModulo(int Value, int Divisor)
{
    int Result = Value % Divisor;
    return Result < 0 ? Result + Divisor : Result;
}

int ApplyAddressMode(int Coordinate, int Size, uint AddressMode)
{
    if (AddressMode == ADDRESS_CLAMP)
    {
        return clamp(Coordinate, 0, Size - 1);
    }

    if (AddressMode == ADDRESS_MIRROR)
    {
        int Period = Size * 2;
        int Wrapped = PositiveModulo(Coordinate, Period);

        return Wrapped < Size ? Wrapped : Period - 1 - Wrapped;
    }

    return PositiveModulo(Coordinate, Size);
}

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

    float4 FilteredValue = float4(0.0);

    for (int Y = 0; Y < 4; ++Y)
    {
        for (int X = 0; X < 4; ++X)
        {
            int2 UnaddressedCoord = SourceBase + int2(X, Y);

            int2 SourceCoord = int2(
                ApplyAddressMode(UnaddressedCoord.x, int(Push.SourceWidth), Push.AddressModeU),
                ApplyAddressMode(UnaddressedCoord.y, int(Push.SourceHeight), Push.AddressModeV)
            );
            
            float Weight = Weights[X] * Weights[Y];

            FilteredValue += SourceMip.Load(int3(SourceCoord, 0)) * Weight;
        }
    }

    DestinationMip[DestinationCoord] = FilteredValue / 64.0;
}