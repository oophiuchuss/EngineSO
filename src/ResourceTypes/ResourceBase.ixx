module;

#include <string>

export module ResourceBase;

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

	void Unload()
	{
		if (!bIsLoaded)
		{
			return; // Not loaded, nothing to unload
		}

		UnloadResource();
		bIsLoaded = false;
	}

protected:
	virtual ~ResourceBase() = default; // Ensure proper cleanup in derived classes

	virtual bool LoadResource(const std::string& FilePath) = 0;
	virtual void UnloadResource() = 0;

private:
	std::string ResourceID;
	bool bIsLoaded = false;
};