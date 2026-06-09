module;

#include <string>
#include <vector>
#include <optional>
#include <glm/glm.hpp>

export module SceneData;

import ResourceBase;

export struct SceneMeshEntry
{
	std::string MeshID;
	std::string MaterialID;
	glm::mat4 LocalTransform = glm::mat4(1.0f);
	std::string Name;
	std::optional<int> ParentIndex; // index into MeshEntries - none if root
	std::vector<int> ChildIndices;
};

export class SceneData : public ResourceBase
{
public:
	explicit SceneData(const std::string& ID) : ResourceBase(ID) {}

	// Populates MeshEntries, registers sub-resources
	// Implementation decides how to access ResourceManager
	virtual void Instantiate() = 0;

	inline const std::vector<SceneMeshEntry>& GetMeshEntries() const { return MeshEntries; }
	inline bool IsInstantiated() const { return bIsInstantiated; }

protected:
	// Pure virtual — subclass knows the format, not SceneData
	bool LoadResource(const std::string& FilePath) override = 0;

	void UnloadResource() override
	{
		MeshEntries.clear();
		bIsInstantiated = false;
	}

	std::vector<SceneMeshEntry> MeshEntries;
	bool bIsInstantiated = false;
};