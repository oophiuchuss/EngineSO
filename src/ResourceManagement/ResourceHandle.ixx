module;

#include <string>

export module ResourceHandle;

export class ResourceManager;

export template<typename T>
class ResourceHandle
{
public:
	ResourceHandle() : ResourceManager(nullptr) {}

	ResourceHandle(const std::string& InResourceID, ResourceManager* InResourceManager)
		: ResourceID(InResourceID), ResourceManager(InResourceManager) {
	}

	T* Get() const;

	bool IsValid() const
	{
		return Get() != nullptr;
	}

	const std::string& GetResourceID() const
	{
		return ResourceID;
	}

	T* operator->() const
	{
		return Get();
	}

	T& operator*() const
	{
		return *Get();
	}

	operator bool() const
	{
		return IsValid();
	}

private:
	std::string ResourceID;
	ResourceManager* ResourceManager;
};

