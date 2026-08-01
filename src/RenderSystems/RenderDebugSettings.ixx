module;

#include <cstdint>

export module RenderDebugSettings;

export enum class RenderDebugView : uint32_t
{
    None = 0,
    MotionVectors = 1
};

export struct RenderDebugSettings
{
    RenderDebugView View = RenderDebugView::None;

    // Affects visualization only
    float MotionVectorScale = 20.0f;

    // Shows raw projection jitter
    bool bPreviewProjectionJitter = false;
};