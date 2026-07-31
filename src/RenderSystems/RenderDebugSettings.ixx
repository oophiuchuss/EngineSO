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

    // Affects visualization only. It never modifies stored motion vectors.
    float MotionVectorScale = 20.0f;
};