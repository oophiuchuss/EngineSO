module;

#include <string>
#include <unordered_map>
#include <typeindex>
#include <memory>

export module ResourceManager;

import ResourceBase;
import Texture;
import Mesh;
import Shader;

// Forward declaration for handle (defined later in same module)
export template<typename T> class ResourceHandle;

// ------------------------------------------------------------------
// ResourceData (internal)
struct ResourceData
{
	std::shared_ptr<ResourceBase> Resource;
	int RefCount = 0;

	// Maybe add override operator ++ and -- for easier reference counting + add getter for resource 
};

// ------------------------------------------------------------------
// ResourceManager (exported)
export class ResourceManager
{
public:
	template<typename T, typename ...Args>
	ResourceHandle<T> Load(const std::string& ResourceID, Args && ...args);

	void Release(const std::string& ResourceID, const std::type_index& ResourceType);

	void UnloadAll();

	template<typename T>
	T* GetResource(const std::string& ResourceID);

	ResourceBase* GetResourceByType(const std::string& ResourceID, const std::type_index& ResourceType);

	template<typename T>
	bool HasResource(const std::string& ResourceID) const;
	
	template<typename T>
	bool HasResourceType() const;

protected:
	// Convert type to folder name and file extension
	std::type_index GetAssetType(const std::string& FilePath) const;

	// Convert type to folder name and file extension
	template<typename T>
	std::string GetAssetFolder() const;

	template<typename T>
	std::string GetFileExtension() const;

	// Build full file path from resource ID and type
	template<typename T>
	std::string GetResourceFilePath(const std::string& ResourceID) const;

	std::unordered_map<std::type_index,
		std::unordered_map<std::string, ResourceData>> Resources;
};


// TODO: upgrades
// Dangling raw pointers from GetResource
// Release linear search over all types
// No thread safety(refcount, map access)
// Load does not re‑load invalid resources
// UnloadAll leaves handles dangling
// Refcount can go negative
// ResourceData lacks encapsulation
// Missing copy / move control for manager


// TODO: Is resource handle even needed?

// ------------------------------------------------------------------
// ResourceHandle (exported, defined after ResourceManager)
export template<typename T>
class ResourceHandle {
public:
	ResourceHandle() : ResourceManagerPtr(nullptr) {}
	ResourceHandle(const std::string& InResourceID, ResourceManager* InResourceManager)
		: ResourceID(InResourceID), ResourceManagerPtr(InResourceManager) {
	}

	T* Get() const {
		if (!ResourceManagerPtr) return nullptr;
		return ResourceManagerPtr->GetResource<T>(ResourceID);
	}

	bool IsValid() const { return Get() != nullptr; }
	const std::string& GetResourceID() const { return ResourceID; }
	T* operator->() const { return Get(); }
	T& operator*() const { return *Get(); }
	operator bool() const { return IsValid(); }

private:
	std::string ResourceID;
	ResourceManager* ResourceManagerPtr = nullptr;
};

// ------------------------------------------------------------------
// ResourceManager method implementations (must be in the same module unit)
template<typename T, typename... Args>
ResourceHandle<T> ResourceManager::Load(const std::string& ResourceID, Args&&... args)
{
	static_assert(std::is_base_of_v<ResourceBase, T>, "T must derive from ResourceBase");

	auto& TypeResources = Resources[std::type_index(typeid(T))];
	auto It = TypeResources.find(ResourceID);

	if (It != TypeResources.end()) {
		It->second.RefCount++;
		return ResourceHandle<T>(ResourceID, this);
	}

	// Create new resource
	auto Resource = std::make_shared<T>(ResourceID, std::forward<Args>(args)...);
	std::string FilePath = GetResourceFilePath<T>(ResourceID);
	if (!Resource->Load(FilePath)) {

		// TODO: Shouldn't be destroyed if load failed?
		return ResourceHandle<T>();
	}

	TypeResources[ResourceID] = { Resource, 1 };
	return ResourceHandle<T>(ResourceID, this);
}

template<typename T>
T* ResourceManager::GetResource(const std::string& ResourceID) 
{
	static_assert(std::is_base_of_v<ResourceBase, T>, "T must derive from ResourceBase");

	auto& TypeResources = Resources[std::type_index(typeid(T))];
	auto It = TypeResources.find(ResourceID);

	if (It != TypeResources.end())
	{
		return static_cast<T*>(It->second.Resource.get());
	}

	// Resource not found or invalid
	return nullptr;
}

template<typename T>
bool ResourceManager::HasResource(const std::string& ResourceID) const
{
	static_assert(std::is_base_of_v<ResourceBase, T>, "T must derive from ResourceBase");

	auto TypeIt = Resources.find(std::type_index(typeid(T)));
	if (TypeIt != Resources.end())
	{
		const auto& TypeResources = TypeIt->second;
		return TypeResources.find(ResourceID) != TypeResources.end();
	}
	return false;
}

template<typename T>
bool ResourceManager::HasResourceType() const
{
	static_assert(std::is_base_of_v<ResourceBase, T>, "T must derive from ResourceBase");

	auto TypeIt = Resources.find(std::type_index(typeid(T)));
	return TypeIt != Resources.end();
}

template<typename T>
std::string ResourceManager::GetAssetFolder() const
{
	if constexpr (std::is_same_v<T, Texture>) return "textures";
	else if constexpr (std::is_same_v<T, Mesh>) return "meshes";
	else if constexpr (std::is_same_v<T, Shader>) return "shaders";
	else return "";
}

template<typename T>
std::string ResourceManager::GetFileExtension() const
{
	if constexpr (std::is_same_v<T, Texture>) return ".ktx";
	else if constexpr (std::is_same_v<T, Mesh>) return ".obj";
	else if constexpr (std::is_same_v<T, Shader>) return ".spv";
	else return "";
}

template<typename T>
std::string ResourceManager::GetResourceFilePath(const std::string& ResourceID) const
{
	return "assets/" + GetAssetFolder<T>() + "/" + ResourceID + GetFileExtension<T>();
}
