import CameraUBO;

// Set 0: per-frame camera data.
[[vk::binding(0, 0)]] ConstantBuffer<CameraUBO> uCamera;

// Set 1: temporal inputs.
[[vk::binding(0, 1)]] Sampler2D CurrentColorTexture;
[[vk::binding(1, 1)]] Sampler2D CurrentDepthTexture;
[[vk::binding(2, 1)]] Sampler2D VelocityTexture;
[[vk::binding(3, 1)]] Sampler2D HistoryColorTexture;
[[vk::binding(4, 1)]] Sampler2D HistoryDepthTexture;

static const uint32_t TAA_HistoryValid = 1u << 0;
static const uint32_t TAA_AccumulationEnabled = 1u << 1;

static const float BackgroundDeviceDepth = 1.0;
static const float MotionValidityThreshold = 0.5;
static const float MinimumClipW = 0.00001;
static const float MinimumLuminanceScale = 0.1;
static const float StationaryThresholdPixels = 0.1;
static const float FullResponseVelocityPixels = 8.0;

static const float3 LuminanceCoefficients = float3(0.2126, 0.7152, 0.0722);


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


// General utilities
float2 GetTexelSize()
{
    return float2(pc.InverseWidth, pc.InverseHeight);
}


bool IsInsideImage(float2 UV)
{
    return all(UV >= float2(0.0, 0.0)) && all(UV <= float2(1.0, 1.0));
}


bool IsDeviceDepthValid(float Depth)
{
    return Depth >= 0.0 && Depth <= 1.0;
}


float2 ClipPositionToUV(float4 ClipPosition)
{
    const float2 NDC = ClipPosition.xy / ClipPosition.w;

    return NDC * 0.5 + 0.5;
}


float3 ReconstructWorldPosition(float2 UV, float Depth)
{
    float4 ClipPosition;

    ClipPosition.xy = UV * 2.0 - 1.0;
    ClipPosition.z = Depth;
    ClipPosition.w = 1.0;

    const float4 WorldPosition = mul(uCamera.InverseViewProj, ClipPosition);

    return WorldPosition.xyz / WorldPosition.w;
}


// Color reconstruction and validation

float Luminance(float3 Color)
{
    const float3 NonNegativeColor = max(Color, float3(0.0, 0.0, 0.0));

    return dot(NonNegativeColor, LuminanceCoefficients);
}


float ComputeHistoryAgreement(float3 CurrentColor, float3 HistoryColor)
{
    const float CurrentLuminance = Luminance(CurrentColor);
    const float HistoryLuminance = Luminance(HistoryColor);

    const float LuminanceScale = max(max(CurrentLuminance, HistoryLuminance), MinimumLuminanceScale);

    const float RelativeDifference = abs(CurrentLuminance - HistoryLuminance) / LuminanceScale;

    return 1.0 - saturate(RelativeDifference);
}


void ComputeCurrentNeighborhoodBounds(float2 UV, float3 CenterColor, out float3 NeighborhoodMinimum, out float3 NeighborhoodMaximum)
{
    const float2 TexelSize = GetTexelSize();

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

            const float2 Offset = float2(float(X), float(Y)) * TexelSize;

            const float3 NeighborColor = CurrentColorTexture.Sample(UV + Offset).rgb;

            NeighborhoodMinimum = min(NeighborhoodMinimum, NeighborColor);

            NeighborhoodMaximum = max(NeighborhoodMaximum, NeighborColor);
        }
    }
}


float3 SampleHistoryCatmullRom(float2 UV)
{
    const float2 TexelSize = GetTexelSize();
    const float2 TextureSize = 1.0 / TexelSize;

    // Convert normalized UV coordinates into texel space.
    const float2 SamplePosition = UV * TextureSize;

    // Center of the texel immediately before the interpolation point.
    const float2 TexelPosition1 = floor(SamplePosition - 0.5) + 0.5;

    const float2 Fraction = SamplePosition - TexelPosition1;

    // Catmull-Rom weights for four logical texels on each axis.
    const float2 Weight0 = Fraction * (-0.5 + Fraction * (1.0 - 0.5 * Fraction));

    const float2 Weight1 = 1.0 + Fraction * Fraction * (-2.5 + 1.5 * Fraction);

    const float2 Weight2 = Fraction * (0.5 + Fraction * (2.0 - 1.5 * Fraction));

    const float2 Weight3 = Fraction * Fraction * (-0.5 + 0.5 * Fraction);

    // Linear filtering combines the two middle logical samples.
    const float2 Weight12 = Weight1 + Weight2;
    const float2 Offset12 = Weight2 / Weight12;

    const float2 TexelPosition0 = TexelPosition1 - 1.0;
    const float2 TexelPosition3 = TexelPosition1 + 2.0;
    const float2 TexelPosition12 = TexelPosition1 + Offset12;

    const float2 UV0 = TexelPosition0 * TexelSize;
    const float2 UV3 = TexelPosition3 * TexelSize;
    const float2 UV12 = TexelPosition12 * TexelSize;

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


// Motion and depth validation

float4 SelectClosestSurfaceMotion(float2 UV, float CenterDepth)
{
    const float2 TexelSize = GetTexelSize();

    float ClosestDepth = CenterDepth;
    float2 ClosestUV = UV;

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

            const float2 CandidateUV = UV + float2(float(X), float(Y)) * TexelSize;

            const float CandidateDepth = CurrentDepthTexture.Sample(CandidateUV).r;

            // Regular depth: zero is near, one is the background.
            if (CandidateDepth < ClosestDepth)
            {
                ClosestDepth = CandidateDepth;
                ClosestUV = CandidateUV;
            }
        }
    }

    return VelocityTexture.Sample(ClosestUV);
}


float ComputeMinimumHistoryDepthDifference(float2 HistoryUV, float ExpectedPreviousDepth)
{
    const float2 TexelSize = GetTexelSize();

    float MinimumDifference = 1.0;

    [unroll]
    for (int Y = -1; Y <= 1; Y++)
    {
        [unroll]
        for (int X = -1; X <= 1; X++)
        {
            const float2 SampleUV = HistoryUV + float2(float(X), float(Y)) * TexelSize;

            const float NeighborDepth = HistoryDepthTexture.Sample(SampleUV).r;

            const float Difference = abs(ExpectedPreviousDepth - NeighborDepth);

            MinimumDifference = min(MinimumDifference, Difference);
        }
    }

    return MinimumDifference;
}


// History reprojection

bool ComputeBackgroundHistoryUV(float2 CurrentUV, out float2 HistoryUV)
{
    // Safe fallback and definite initialization for every return path.
    HistoryUV = CurrentUV;

    const float3 CurrentFarWorldPosition = ReconstructWorldPosition(CurrentUV, BackgroundDeviceDepth);

    const float3 WorldDirection = normalize(CurrentFarWorldPosition - uCamera.CameraPos.xyz);

    // Translation must not affect an infinitely distant background.
    const float3 PreviousWorldPosition = uCamera.PreviousCameraPos.xyz + WorldDirection;

    const float4 PreviousClipPosition = mul(uCamera.PreviousViewProj, float4(PreviousWorldPosition, 1.0));

    if (PreviousClipPosition.w <= MinimumClipW)
    {
        return false;
    }

    const float2 PreviousUV = ClipPositionToUV(PreviousClipPosition);

    const float2 CurrentJitterUV = uCamera.TemporalJitterUV.xy;

    const float2 PreviousJitterUV = uCamera.TemporalJitterUV.zw;

    const float2 JitterDeltaUV = CurrentJitterUV - PreviousJitterUV;

    // PreviousUV contains previous projection jitter.
    // Shift it into the stable temporal output grid.
    HistoryUV = PreviousUV + JitterDeltaUV;

    return true;
}


bool ResolveHistoryReprojection(float2 CurrentUV, float CurrentDepth, out float2 HistoryUV, out float ExpectedPreviousDepth)
{
    HistoryUV = CurrentUV;
    ExpectedPreviousDepth = BackgroundDeviceDepth;

    const float4 MotionData = SelectClosestSurfaceMotion(CurrentUV, CurrentDepth);

    const bool MotionIsValid = MotionData.w >= MotionValidityThreshold;

    if (MotionIsValid)
    {
        HistoryUV = CurrentUV - MotionData.xy;
        ExpectedPreviousDepth = MotionData.z;
    }
    else
    {
        // Invalid geometry motion is accepted only for untouched background.
        // Other unsupported cases reject history.
        const bool IsBackground = CurrentDepth >= BackgroundDeviceDepth;

        if (!IsBackground)
        {
            return false;
        }

        if (!ComputeBackgroundHistoryUV(CurrentUV, HistoryUV))
        {
            return false;
        }
    }

    return IsInsideImage(HistoryUV) && IsDeviceDepthValid(ExpectedPreviousDepth);
}


// Entry point

[shader("fragment")]
PSOutput main(PSInput input)
{
    const float4 CurrentColor = CurrentColorTexture.Sample(input.uv);

    const float CurrentDepth = CurrentDepthTexture.Sample(input.uv).r;

    // Every rejection path falls back to the unaccumulated current frame.
    PSOutput output;
    output.HistoryColor = CurrentColor;
    output.HistoryDepth = CurrentDepth;

    const bool HistoryIsValid = (pc.Flags & TAA_HistoryValid) != 0;

    const bool AccumulationIsEnabled = (pc.Flags & TAA_AccumulationEnabled) != 0;

    if (!HistoryIsValid || !AccumulationIsEnabled)
    {
        return output;
    }

    float2 HistoryUV;
    float ExpectedPreviousDepth;

    if (!ResolveHistoryReprojection(input.uv, CurrentDepth, HistoryUV, ExpectedPreviousDepth))
    {
        return output;
    }

    const float2 TexelSize = GetTexelSize();

    const float2 CompensatedVelocityUV = input.uv - HistoryUV;

    const float2 CompensatedVelocityPixels = CompensatedVelocityUV / TexelSize;

    const float MotionMagnitudePixels = length(CompensatedVelocityPixels);

    // Stationary silhouette depth changes are subpixel coverage samples,
    // not real disocclusions. Moving pixels still receive conservative
    // history-depth validation.
    if (MotionMagnitudePixels > StationaryThresholdPixels)
    {
        const float MinimumDepthDifference = ComputeMinimumHistoryDepthDifference(HistoryUV, ExpectedPreviousDepth);

        const float DepthTolerance = max(pc.DepthTolerance, 0.0);

        if (MinimumDepthDifference > DepthTolerance)
        {
            return output;
        }
    }

    const float3 ReconstructedHistoryColor = SampleHistoryCatmullRom(HistoryUV);

    float3 NeighborhoodMinimum;
    float3 NeighborhoodMaximum;

    ComputeCurrentNeighborhoodBounds(
        input.uv,
        CurrentColor.rgb,
        NeighborhoodMinimum,
        NeighborhoodMaximum);

    // Catmull-Rom can overshoot because it has negative filter lobes.
    // Neighborhood clamping also protects against stale history.
    const float3 ClampedHistoryColor = clamp(ReconstructedHistoryColor, NeighborhoodMinimum, NeighborhoodMaximum);

    const float MaximumHistoryWeight = saturate(pc.HistoryWeight);

    const float MinimumHistoryWeight = min(saturate(pc.ResponsiveHistoryWeight), MaximumHistoryWeight);

    const float HistoryAgreement = ComputeHistoryAgreement(CurrentColor.rgb, ClampedHistoryColor);

    const float MotionAmount = saturate(MotionMagnitudePixels / FullResponseVelocityPixels);

    const float Disagreement = 1.0 - HistoryAgreement;

    // Static subpixel disagreement is accumulated.
    // Moving disagreement becomes responsive.
    const float ResponsiveAmount = MotionAmount * Disagreement;

    const float AdaptiveHistoryWeight = lerp(MaximumHistoryWeight, MinimumHistoryWeight, ResponsiveAmount);

    const float3 ResolvedColor = lerp(CurrentColor.rgb, ClampedHistoryColor, AdaptiveHistoryWeight);

    output.HistoryColor = float4(ResolvedColor, CurrentColor.a);

    return output;
}