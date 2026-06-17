module;

#include <vector>
#include <glm/glm.hpp>

export module FrameData;

import Geometry;
import Mesh;
import CameraUniform;

export struct RenderableMesh
{
    Mesh* GPUMesh = nullptr;
    BoundingBox WorldBounds;
    uint32_t ObjectIndex = 0; // index into this frame's ObjectData/MaterialData SSBOs
};

export struct FrameData
{
    std::vector<RenderableMesh> Renderables;
    CameraUniformData Camera;
};