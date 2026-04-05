module;

#include <filesystem>
#include <thread>

module HotReloadResourceManager;

import ResourceBase;

void HotReloadResourceManager::StartWatcher()
{
	bIsRunning = true;
	WatcherThread = std::thread([this]() {WatcherFunction(); });
}

void HotReloadResourceManager::StopWatcher()
{
	bIsRunning = false;
	if (WatcherThread.joinable())
	{
		WatcherThread.join();
	}
}

void HotReloadResourceManager::WatcherFunction()
{
	while (bIsRunning)
	{
		for(auto& [FilePath, LastTimestamp] : FileTimestamps)
		{
			try
			{
				auto CurrentTimestamp = std::filesystem::last_write_time(FilePath);
				if (CurrentTimestamp != LastTimestamp)
				{
					ReloadResource(FilePath);
					LastTimestamp = CurrentTimestamp;
				}
			}
			catch (const std::filesystem::filesystem_error&)
			{
				// Handle missing file case (e.g., log warning, set timestamp to epoch, etc.)
			}
		}

		std::this_thread::sleep_for(std::chrono::seconds(1)); // Check every second (adjust as needed)
	}
}

void HotReloadResourceManager::ReloadResource(const std::string& FilePath)
{
	std::filesystem::path Path(FilePath);
	std::string Filename = Path.stem().string();

	std::type_index TypeIndex = GetAssetType(FilePath);

	if (TypeIndex == std::type_index(typeid(ResourceBase)))
	{
		return; // Unknown resource type, can't reload
	}

	ResourceBase* FoundResource = GetResourceByType(Filename, TypeIndex);
	if (!FoundResource)
	{
		return; // Resource not found, can't reload
	}

	FoundResource->Unload();
	FoundResource->Load(FilePath);
}
