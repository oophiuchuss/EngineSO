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

export struct RenderableMesh
{
    Mesh* GPUMesh = nullptr;
    BoundingBox WorldBounds;
    uint32_t ObjectIndex = 0; // index into this frame's ObjectData/MaterialData SSBOs
};

export struct FrameData
{
    uint32_t FrameIndex = 0;

    std::vector<RenderableMesh> Renderables;
    std::vector<RenderableMesh> TranslucentRenderables;

    CameraUniformData Camera;
    EnvironmentUniformData Environment;
    RenderDebugSettings DebugSettings;

    ImDrawData* ImGuiDrawData = nullptr;
};