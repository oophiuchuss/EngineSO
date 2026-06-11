module;

#include <thread>
#include <queue>
#include <functional>
#include <mutex>

export module AsyncResourceLoader;

import ResourceManager;
import ResourceHandle;

export class AsyncResourceLoader
{
public:
	AsyncResourceLoader(ResourceManager* InResourceManager) : ResourceManagerPtr(InResourceManager) 
	{
		Start();
	}

	~AsyncResourceLoader()
	{
		Stop();
	}

	void Start();

	void Stop();

	template<typename T>
	void LoadAsync(const std::string& ResourceID, std::function<void(ResourceHandle<T>)> LoadFunction)
	{
		std::lock_guard<std::mutex> Lock(QueueMutex);
		TaskQueue.push([this, ResourceID, LoadFunction]() 
			{
				ResourceHandle<T> Handle = ResourceManagerPtr->Load<T>(ResourceID);
				LoadFunction(Handle);
			});

		Condition.notify_one();
	}

private:
	void WorkerFunction();

	ResourceManager* ResourceManagerPtr;
	std::thread WorkerThread;
	std::queue<std::function<void()>> TaskQueue;
	std::mutex QueueMutex;
	std::condition_variable Condition;
	bool bIsRunning = false;
};