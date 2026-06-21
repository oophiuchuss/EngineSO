module;

#include <filesystem>
#include <string>
#include <unordered_map>
#include <thread>

export module HotReloadResourceManager;

export import ResourceManager;

import TaskScheduler;

export class HotReloadResourceManager : public ResourceManager
{
public:
	HotReloadResourceManager(TaskScheduler& Scheduler) : ResourceManager(Scheduler)
	{
		StartWatcher();
	}

	~HotReloadResourceManager()
	{
		StopWatcher();
	}

	void StartWatcher();
	void StopWatcher();

	template<typename T, typename ...Args>
	ResourceHandle<T> Load(const std::string& ResourceID, Args && ...args)
	{
		auto Handle = ResourceManager::Load<T>(ResourceID, std::forward<Args>(args)...);
		if (Handle.IsValid())
		{
			std::string FilePath = GetResourceFilePath<T>(ResourceID);
			try
			{
				FileTimestamps[FilePath] = std::filesystem::last_write_time(FilePath);
			}
			catch (const std::filesystem::filesystem_error&)
			{
				// Handle missing file case (e.g., log warning, set timestamp to epoch, etc.)
			}
		}
		return Handle;
	}

private:
	void WatcherFunction();

	void ReloadResource(const std::string& FilePath);

	std::unordered_map<std::string, std::filesystem::file_time_type> FileTimestamps; // Map of file paths to their last known timestamps
	std::thread WatcherThread;
	bool bIsRunning = true;
};