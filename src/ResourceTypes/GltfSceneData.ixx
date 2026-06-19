module;

#include <string>
#include <vector>
#include <optional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// fastgltf
#include <fastgltf/core.hpp> // TOOD: maybe should be removed

export module GltfSceneData;

import SceneData;
import ResourceManager;
import TaskScheduler;

import TextureData; // TOOD: maybe should be removed

export class GltfSceneData : public SceneData
{
public:
	explicit GltfSceneData(
		const std::string& ID,
		ResourceManager& RM,
		TaskScheduler& TS) :
		SceneData(ID),
		ResourceManagerRef(RM),
		TaskSchedulerRef(TS)
	{}

	void Instantiate() override;

protected:
	bool LoadResource(const std::string& FilePath) override;
	void UnloadResource() override;

private:
	// Raw parsed data — populated by LoadResource, cleared after Instantiate
	struct RawPrimitive
	{
		std::vector<glm::vec3> Positions;
		std::vector<glm::vec2> UVs;
		std::vector<glm::vec3> Normals;
		std::vector<uint32_t> Indices;
		int MaterialIndex = -1; // -1 if no material

	};
	
	struct RawMesh
	{
		std::string Name;
		std::vector<RawPrimitive> Primitives;
	};
	
	struct RawTexture
	{
		std::string Name;
		std::string FilePath;				// external - URI based
		std::vector<uint8_t> EmbeddedData;	// internal - buffer based
		bool bIsEmbedded = false;
	};
	
	struct RawMaterial
	{
		std::string Name;

		// PBR factors
		glm::vec4 AlbedoFactor = glm::vec4(1.0f);
		float MetallicFactor = 0.0f;
		float RoughnessFactor = 0.5f;
		glm::vec3 EmissiveFactor = glm::vec3(0.0f);
		float NormalScale = 1.0f;
		float OcclusionStrength = 1.0f;

		// texture indicies into raw textures
		// -1 means no texture
		int AlbedoTextureIndex = -1;
		int NormalTextureIndex = -1;
		int MetallicRoughnessTextureIndex = -1;
		int OcclusionTextureIndex = -1;
		int EmissiveTextureIndex = -1;
	};
	
	struct RawNode
	{
		std::string Name;
		glm::mat4 LocalTransform = glm::mat4(1.0f);
		int MeshIndex = -1; // -1 if no mesh
		std::optional<int> ParentIndex;
		std::vector<int> ChildIndices;
	};

	struct TextureDecodeResult
	{
		int Index;                     // position in RawTextures
		std::vector<uint8_t> Pixels;   // decoded RGBA data
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Channels = 0;
		bool Success = false;
	};


	// Helper methods
	glm::mat4 ResolveNodeTransform(const fastgltf::Node& Node) const;
	std::vector<std::string> RegisterAllTextures();
	std::vector<std::string> RegisterAllMaterials(const std::vector<std::string>& TextureIDs);
	void RegisterAllNodes(const std::vector<std::string>& MaterialIDs);

	std::vector<RawMesh> RawMeshes;
	std::vector<RawMaterial> RawMaterials;
	std::vector<RawTexture> RawTextures;
	std::vector<RawNode> RawNodes;

	ResourceManager& ResourceManagerRef;
	TaskScheduler& TaskSchedulerRef;
};