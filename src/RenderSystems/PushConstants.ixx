module;

#include <glm/glm.hpp>

export module PushConstants;

import MaterialProperties;

// Must match PushConstants.slang exactly — same field order, same sizes
// Total size: 64 + 16 + 4 + 4 + 4 + 4 = 96 bytes
// Vulkan guarantees minimum 128 bytes for push constants

export struct PushConstantData
{
	glm::mat4 ModelMatrix = glm::mat4(1.0f);
	MaterialProperties Material;
	float _Padding = 0.0f; // explicit padding — keeps GPU alignment correct
};