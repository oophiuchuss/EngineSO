[[vk::binding(0, 0)]] Sampler2D CurrentColorTexture;
[[vk::binding(1, 0)]] Sampler2D CurrentDepthTexture;
[[vk::binding(2, 0)]] Sampler2D VelocityTexture;
[[vk::binding(3, 0)]] Sampler2D HistoryColorTexture;
[[vk::binding(4, 0)]] Sampler2D HistoryDepthTexture;

static const uint32_t TAA_HistoryValid = 1u << 0;
static const uint32_t TAA_AccumulationEnabled = 1u << 1;

struct PushConstants
{
    float HistoryWeight;
    float ResponsiveHistoryWeight;

    float DepthTolerance;
    float InverseWidth;

    float InverseHeight;
    uint32_t Flags;
};

[[vk::push_constant]] PushConstants pc;

struct PSInput
{
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};

struct PSOutput
{
    float4 HistoryColor : SV_Target0;
    float HistoryDepth : SV_Target1;
};

bool IsInsideImage(float2 UV)
{
    return all(UV >= float2(0.0, 0.0)) && all(UV <= float2(1.0, 1.0));
}

bool IsDeviceDepthValid(float Depth)
{
    return Depth >= 0.0 && Depth <= 1.0;
}

float Luminance(float3 Color)
{
    return dot(max(Color, float3(0.0, 0.0, 0.0)), float3(0.2126, 0.7152, 0.0722));
}

float ComputeHistoryAgreement(float3 CurrentColor, float3 HistoryColor)
{
    float CurrentLuminance = Luminance(CurrentColor);
    float HistoryLuminance = Luminance(HistoryColor);

    float LuminanceScale = max(max(CurrentLuminance, HistoryLuminance), 0.1);

    float RelativeDifference = abs(CurrentLuminance - HistoryLuminance) / LuminanceScale;

    return 1.0 - saturate(RelativeDifference);
}

void ComputeCurrentNeighborhoodBounds(float2 UV, float3 CenterColor, out float3 NeighborhoodMinimum, out float3 NeighborhoodMaximum)
{
    float2 TexelSize = float2(pc.InverseWidth, pc.InverseHeight);

    NeighborhoodMinimum = CenterColor;
    NeighborhoodMaximum = CenterColor;

    [unroll]
    for (int Y = -1; Y <= 1; Y++)
    {
        [unroll]
        for (int X = -1; X <= 1; X++)
        {
            if (X == 0 && Y == 0)
            {
                continue;
            }

            float2 Offset = float2(float(X), float(Y)) * TexelSize;

            float3 NeighborColor = CurrentColorTexture.Sample(UV + Offset).rgb;

            NeighborhoodMinimum = min(NeighborhoodMinimum, NeighborColor);

            NeighborhoodMaximum = max(NeighborhoodMaximum, NeighborColor);
        }
    }
}

float3 SampleHistoryCatmullRom(float2 UV)
{
    float2 TexelSize = float2(pc.InverseWidth, pc.InverseHeight);

    float2 TextureSize = 1.0 / TexelSize;

    // Convert the normalized coordinate into texel space.
    float2 SamplePosition = UV * TextureSize;

    // Center of the texel immediately before the interpolation point.
    float2 TexelPosition1 = floor(SamplePosition - 0.5) + 0.5;

    float2 Fraction = SamplePosition - TexelPosition1;

    // Catmull–Rom weights for four texels on each axis.
    float2 Weight0 = Fraction * (-0.5 + Fraction * (1.0 - 0.5 * Fraction));

    float2 Weight1 = 1.0 + Fraction * Fraction * (-2.5 + 1.5 * Fraction);

    float2 Weight2 = Fraction * (0.5 + Fraction * (2.0 - 1.5 * Fraction));

    float2 Weight3 = Fraction * Fraction * (-0.5 + 0.5 * Fraction);

    // The two middle samples can be combined through linear filtering.
    float2 Weight12 = Weight1 + Weight2;
    float2 Offset12 = Weight2 / Weight12;

    float2 TexelPosition0 = TexelPosition1 - 1.0;
    float2 TexelPosition3 = TexelPosition1 + 2.0;
    float2 TexelPosition12 = TexelPosition1 + Offset12;

    float2 UV0 = TexelPosition0 * TexelSize;
    float2 UV3 = TexelPosition3 * TexelSize;
    float2 UV12 = TexelPosition12 * TexelSize;

    float3 Result = float3(0.0, 0.0, 0.0);

    Result += HistoryColorTexture.Sample(float2(UV0.x, UV0.y)).rgb * Weight0.x * Weight0.y;
    Result += HistoryColorTexture.Sample(float2(UV12.x, UV0.y)).rgb * Weight12.x * Weight0.y;
    Result += HistoryColorTexture.Sample(float2(UV3.x, UV0.y)).rgb * Weight3.x * Weight0.y;
    Result += HistoryColorTexture.Sample(float2(UV0.x, UV12.y)).rgb * Weight0.x * Weight12.y;
    Result += HistoryColorTexture.Sample(float2(UV12.x, UV12.y)).rgb * Weight12.x * Weight12.y;
    Result += HistoryColorTexture.Sample(float2(UV3.x, UV12.y)).rgb * Weight3.x * Weight12.y;
    Result += HistoryColorTexture.Sample(float2(UV0.x, UV3.y)).rgb * Weight0.x * Weight3.y;
    Result += HistoryColorTexture.Sample(float2(UV12.x, UV3.y)).rgb * Weight12.x * Weight3.y;
    Result += HistoryColorTexture.Sample(float2(UV3.x, UV3.y)).rgb * Weight3.x * Weight3.y;

    return Result;
}

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput output;

    float4 CurrentColor = CurrentColorTexture.Sample(input.uv);

    float CurrentDepth = CurrentDepthTexture.Sample(input.uv).r;

    output.HistoryDepth = CurrentDepth;

    const bool HistoryIsValid = (pc.Flags & TAA_HistoryValid) != 0;

    const bool AccumulationIsEnabled = (pc.Flags & TAA_AccumulationEnabled) != 0;

    if (!HistoryIsValid || !AccumulationIsEnabled)
    {
        output.HistoryColor = CurrentColor;
        return output;
    }

    float4 MotionData = VelocityTexture.Sample(input.uv);

    if (MotionData.w < 0.5)
    {
        output.HistoryColor = CurrentColor;
        return output;
    }

    float2 HistoryUV = input.uv - MotionData.xy;

    if (!IsInsideImage(HistoryUV))
    {
        output.HistoryColor = CurrentColor;
        return output;
    }

    float ExpectedPreviousDepth = MotionData.z;

    if (!IsDeviceDepthValid(ExpectedPreviousDepth))
    {
        output.HistoryColor = CurrentColor;
        return output;
    }

    float ActualPreviousDepth = HistoryDepthTexture.Sample(HistoryUV).r;

    float DepthDifference = abs(ExpectedPreviousDepth - ActualPreviousDepth);

    float DepthTolerance = max(pc.DepthTolerance, 0.0);

    if (DepthDifference > DepthTolerance)
    {
        output.HistoryColor = CurrentColor;
        return output;
    }

    float3 ReconstructedHistoryColor = SampleHistoryCatmullRom(HistoryUV);

    float3 NeighborhoodMinimum;
    float3 NeighborhoodMaximum;

    ComputeCurrentNeighborhoodBounds(
        input.uv,
        CurrentColor.rgb,
        NeighborhoodMinimum,
        NeighborhoodMaximum);

    // Catmull–Rom can overshoot because it has negative filter lobes.
    // Neighborhood clamping also protects against stale history.
    float3 ClampedHistoryColor = clamp(ReconstructedHistoryColor, NeighborhoodMinimum, NeighborhoodMaximum);

    float MaximumHistoryWeight = saturate(pc.HistoryWeight);

    float MinimumHistoryWeight = min(saturate(pc.ResponsiveHistoryWeight), MaximumHistoryWeight);

    float HistoryAgreement = ComputeHistoryAgreement(CurrentColor.rgb, ClampedHistoryColor);

    float AdaptiveHistoryWeight = lerp(MinimumHistoryWeight, MaximumHistoryWeight, HistoryAgreement);

    float3 ResolvedColor = lerp(CurrentColor.rgb, ClampedHistoryColor, AdaptiveHistoryWeight);

    output.HistoryColor = float4(ResolvedColor, CurrentColor.a);

    return output;
}