module;

#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <filesystem>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

// fastgltf
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

module GltfSceneData;

void GltfSceneData::Instantiate()
{
}

bool GltfSceneData::LoadResource(const std::string& FilePath)
{
	return false;
}

void GltfSceneData::UnloadResource()
{
}

glm::mat4 GltfSceneData::ResolveNodeTransform(const fastgltf::Node& Node) const
{
	return glm::mat4();
}

std::vector<std::string> GltfSceneData::RegisterAllTextures()
{
	return std::vector<std::string>();
}

std::vector<std::string> GltfSceneData::RegisterAllMaterials(const std::vector<std::string>& TextureIDs)
{
	return std::vector<std::string>();
}

void GltfSceneData::RegisterAllMeshes(const std::vector<std::string>& MaterialIDs)
{
}