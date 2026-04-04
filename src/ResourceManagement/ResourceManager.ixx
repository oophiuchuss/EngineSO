module;

#include <string>
#include <unordered_map>
#include <typeindex>
#include <memory>

export module ResourceManager;

import ResourceBase;
import ResourceHandle;

struct ResourceData
{
	std::shared_ptr<ResourceBase> Resource;
	int RefCount = 0;

	// Maybe add override operator ++ and -- for easier reference counting + add getter for resource 
};

export class ResourceManager
{
public:
	template<typename T, typename ...Args>
	ResourceHandle<T> Load(const std::string& ResourceID, Args && ...args)
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
		if (!Resource->Load()) {
			return ResourceHandle<T>();
		}

		TypeResources[ResourceID] = { Resource, 1 };
		return ResourceHandle<T>(ResourceID, this);
	}


	// TODO: implement release resource by id and type
	void Release(const std::string& ResourceID)
	{
		for (auto& [Type, TypeResources] : Resources)
		{
			auto ResourceIt = TypeResources.find(ResourceID);
			if (ResourceIt != TypeResources.end())
			{
				ResourceIt->second.RefCount--;
				if (ResourceIt->second.RefCount <= 0)
				{
					ResourceIt->second.Resource->Unload();
					TypeResources.erase(ResourceIt);
				}
				return;
			}
		}
	}

	void UnloadAll()
	{
		for (auto& [Type, TypeResources] : Resources)
		{
			for (auto& [ResourceID, ResourceData] : TypeResources)
			{
				ResourceData.Resource->Unload();
			}
			TypeResources.clear();
		}
	}

	template<typename T>
	T* GetResource(const std::string& ResourceID)
	{
		static_assert(std::is_base_of_v<ResourceBase, T>, "T must derive from ResourceBase");

		auto& TypeResources = Resources[std::type_index(typeid(T))];
		auto It = TypeResources.find(ResourceID);

		if (It != TypeResources.end())
		{
			if (It->second.Resource->IsValid())
			{
				return static_cast<T*>(It->second.Resource.get());

			}
		}

		// Resource not found or invalid
		return nullptr; 
	}

	template<typename T>
	bool HasResource(const std::string& ResourceID) const
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
	bool HasResourceType() const
	{
		static_assert(std::is_base_of_v<ResourceBase, T>, "T must derive from ResourceBase");
		
		auto TypeIt = Resources.find(std::type_index(typeid(T)));
		return TypeIt != Resources.end();
	}

private:
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