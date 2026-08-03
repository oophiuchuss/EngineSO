export module TemporalAASettings;

export struct TemporalAASettings
{
    bool bEnabled = true;

    // Portion of the previous frame retained during accumulation.
    // 0.0 = current frame only
    // 0.9 = 10% current + 90% history
    float HistoryWeight = 0.9f;

    // History contribution used when current and historical colors disagree.
    // Lower values respond faster but preserve less temporal stability.
    float ResponsiveHistoryWeight = 0.2f;

    // Compared in Vulkan device-depth space, not world-space distance.
    float DepthTolerance = 0.001f;
};