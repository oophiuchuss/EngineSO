module;

#include <string>
#include <vector>
#include <optional>
#include <glm/glm.hpp>

export module SceneData;

import ResourceBase;
import LightComponentBase;

export struct SceneNodeEntry
{
	std::string        Name;
	glm::mat4          LocalTransform = glm::mat4(1.0f);
	std::optional<int> ParentIndex;
	std::vector<int>   ChildIndices;
};

export struct MeshInstance
{
	int         NodeIndex = -1;
	std::string MeshID;
	std::string MaterialID;
};

export struct LightInstance
{
	int         NodeIndex = -1;
	LightType   Type = LightType::Directional;
	glm::vec3   Color = glm::vec3(1.0f);
	float       Intensity = 1.0f;
	float       Range = 0.0f;
	float       InnerConeAngle = 0.0f;
	float       OuterConeAngle = 0.0f;
};

export class SceneData : public ResourceBase
{
public:
	explicit SceneData(const std::string& ID) : ResourceBase(ID) {}

	// Populates NodeEntries, registers sub-resources
	// Implementation decides how to access ResourceManager
	virtual void Instantiate() = 0;

public:
	inline const std::vector<SceneNodeEntry>& GetNodeEntries() const { return NodeEntries; }
	inline const std::vector<MeshInstance>& GetMeshInstances() const { return MeshInstances; }
	inline const std::vector<LightInstance>& GetLightInstances() const { return LightInstances; }

	inline bool IsInstantiated() const { return bIsInstantiated; }

	static std::string_view AssetFolder() { return "scenes"; }
	static std::string_view FileExtension() { return ""; }

protected:
	// Pure virtual — subclass knows the format, not SceneData
	bool LoadResource(const std::string& FilePath) override = 0;

	void UnloadResource() override
	{
		NodeEntries.clear();
		bIsInstantiated = false;
	}

	std::vector<SceneNodeEntry> NodeEntries;
	std::vector<MeshInstance> MeshInstances;
	std::vector<LightInstance> LightInstances;

	bool bIsInstantiated = false;
};