module;

#include <string>
#include <vector>
#include <optional>
#include <glm/glm.hpp>

export module SceneData;

import ResourceBase;

export struct SceneNodeEntry
{
	std::string        Name;
	glm::mat4          LocalTransform = glm::mat4(1.0f);
	std::optional<int> ParentIndex;
	std::vector<int>   ChildIndices;

	// Mesh data — empty if this is a transform-only node
	bool        bHasMesh = false;
	std::string MeshID;
	std::string MaterialID;
};

export class SceneData : public ResourceBase
{
public:
	explicit SceneData(const std::string& ID) : ResourceBase(ID) {}

	// Populates NodeEntries, registers sub-resources
	// Implementation decides how to access ResourceManager
	virtual void Instantiate() = 0;

	inline const std::vector<SceneNodeEntry>& GetMeshEntries() const { return NodeEntries; }
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
	bool bIsInstantiated = false;
};