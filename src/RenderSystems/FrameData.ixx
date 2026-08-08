module;

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <imgui.h>

export module FrameData;

import Geometry;
import Mesh;
import FrameUniform;

import RenderDebugSettings;
import TemporalAASettings;
import DirectionalShadowSettings;

export struct RenderableMesh
{
    Mesh* GPUMesh = nullptr;
    BoundingBox WorldBounds;
    uint32_t ObjectIndex = 0; // index into this frame's ObjectData/MaterialData SSBOs
};

export struct FrameData
{
    uint32_t FrameIndex = 0;

    bool bTemporalHistoryValid = false;
    uint64_t TemporalFrameIndex = 0;

    std::vector<RenderableMesh> Renderables;
    std::vector<RenderableMesh> TranslucentRenderables;
    std::vector<RenderableMesh> ShadowCasters;

    CameraUniformData Camera;
    EnvironmentUniformData Environment;
    DirectionalShadowUniformData DirectionalShadow;

    TemporalAASettings TemporalSettings;
    RenderDebugSettings DebugSettings;
    DirectionalShadowSettings ShadowSettings;

    ImDrawData* ImGuiDrawData = nullptr;

    bool HasActiveDebugView() const
    {
        return DebugSettings.View != RenderDebugView::None || ShadowSettings.DebugView != DirectionalShadowDebugView::None;
    }
};