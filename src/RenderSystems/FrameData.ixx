module;

#include <vector>
#include <glm/glm.hpp>

export module FrameData;

import Geometry;
import Mesh;
import Shader;
import CameraUniform;

export struct RenderableMesh
{
    Mesh* GPUMesh = nullptr;
    Shader* GPUShader = nullptr;
    glm::mat4   Transform = glm::mat4(1.0f);
    BoundingBox WorldBounds;
};

export struct FrameData
{
    std::vector<RenderableMesh> Renderables;
    CameraUniformData           Camera;
};