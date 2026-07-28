module;

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

export module SceneEnvironment;

import ResourceHandle;
import TextureData;

export struct SceneEnvironment
{
    ResourceHandle<TextureData> Cubemap;

    // Rotation from environment-local space into world space.
    glm::quat Orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    float Intensity = 1.0f;
};