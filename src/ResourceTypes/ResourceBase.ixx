module;

#include <string>
#include <vector>

export module ResourceBase;

import Paths;

export class ResourceBase
{
public:
	explicit ResourceBase(const std::string& InResourceID) : ResourceID(InResourceID) {}

	const std::string& GetResourceID() const { return ResourceID; }
	bool IsLoaded() const { return bIsLoaded; }

	bool Load(const std::string& FilePath)
	{
		if(bIsLoaded)
		{
			return true; // Already loaded
		}

		bIsLoaded = LoadResource(FilePath);
		return bIsLoaded;
	}

	// Memory path — for embedded resources (e.g. textures packed in .glb)
	// Returns false by default — override in resource types that support it
	bool LoadFromMemory(const std::vector<uint8_t>& Data)
	{
		if (bIsLoaded) return true;
		bIsLoaded = LoadResourceFromMemory(Data);
		return bIsLoaded;
	}

	void Unload()
	{
		if (!bIsLoaded)
		{
			return; // Not loaded, nothing to unload
		}

		UnloadResource();
		bIsLoaded = false;
	}

	// Subclasses override to declare where they live
	// Non-virtual intentional — called via T:: in ResourceManager template
	static std::string_view AssetFolder() { return ""; }
	static std::string_view FileExtension() { return ""; }
	static std::string GetRootPath() { return Paths::GetAssetsRoot(); }

protected:
	virtual ~ResourceBase() = default; // Ensure proper cleanup in derived classes

	virtual bool LoadResource(const std::string& FilePath) = 0;
	virtual void UnloadResource() = 0;

	virtual bool LoadResourceFromMemory(const std::vector<uint8_t>& Data)
	{
		return false;
	}

private:
	std::string ResourceID;
	bool bIsLoaded = false;
};