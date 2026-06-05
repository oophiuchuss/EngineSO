module;

#include <glm/glm.hpp>

export module PushConstants;

// Must match PushConstants.slang exactly — same field order, same sizes
// Total size: 64 + 16 + 4 + 4 + 4 + 4 = 96 bytes
// Vulkan guarantees minimum 128 bytes for push constants

export struct MaterialPushData
{
	glm::vec4	AlbedoColor			= glm::vec4(1.0f);
	float		Metallic			= 0.0f;
	float		Roughness			= 1.0f;
	float		EmissiveStrength	= 0.0f;
	float		_Padding			= 0.0f; // explicit padding — keeps GPU alignment correct
};

export struct PushConstantData
{
	glm::mat4 ModelMatrix = glm::mat4(1.0f);
	MaterialPushData Material;
};