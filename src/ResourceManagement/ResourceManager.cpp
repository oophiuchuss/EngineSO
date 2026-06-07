module;

#include <string>
#include <filesystem>

module ResourceManager;

import ResourceBase;
import Texture;
import MeshData;
import ShaderData;

void ResourceManager::Release(const std::string& ResourceID, const std::type_index& ResourceType)
{
	auto TypeIt = Resources.find(ResourceType);
	if (TypeIt != Resources.end())
	{
		auto& TypeResources = TypeIt->second;
		auto ResourceIt = TypeResources.find(ResourceID);
		if (ResourceIt != TypeResources.end())
		{
			ResourceIt->second.RefCount--;
			if (ResourceIt->second.RefCount <= 0)
			{
				ResourceIt->second.Resource->Unload();
				TypeResources.erase(ResourceIt);
			}
		}
	}
}

void ResourceManager::UnloadAll()
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

ResourceBase* ResourceManager::GetResourceByType(const std::string& ResourceID, const std::type_index& ResourceType)
{
	ResourceBase* FoundResource = nullptr;
	auto TypeIt = Resources.find(ResourceType);
	if (TypeIt != Resources.end())
	{
		auto& TypeResources = TypeIt->second;
		auto ResourceIt = TypeResources.find(ResourceID);
		if (ResourceIt != TypeResources.end())
		{
			FoundResource = ResourceIt->second.Resource.get();
		}
	}

	return FoundResource;
}

std::type_index ResourceManager::GetAssetType(const std::string& FilePath) const
{
	std::filesystem::path Path(FilePath);
	std::string ParentFolder = Path.parent_path().filename().string(); 

	if (ParentFolder == "textures")
	{
		return std::type_index(typeid(Texture));
	}
	else if (ParentFolder == "meshes")
	{
		return std::type_index(typeid(MeshData));
	}
	else if (ParentFolder == "shaders")
	{
		return std::type_index(typeid(ShaderData));
	}
	else return std::type_index(typeid(ResourceBase));
}
