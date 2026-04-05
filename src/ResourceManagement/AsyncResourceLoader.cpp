module;

#include <thread>
#include <mutex>

module AsyncResourceLoader;

void AsyncResourceLoader::Start()
{
	bIsRunning = true;

	WorkerThread = std::thread([this]() { WorkerFunction(); });
}

void AsyncResourceLoader::Stop()
{
	{
		std::lock_guard<std::mutex> Lock(QueueMutex);
		bIsRunning = false;
	}
	
	Condition.notify_all();

	if (WorkerThread.joinable())
	{
		WorkerThread.join();
	}
}

void AsyncResourceLoader::WorkerFunction()
{
	while(bIsRunning)
	{
		std::function<void()> Task;
		{
			std::unique_lock<std::mutex> Lock(QueueMutex);
			Condition.wait(Lock, [this]() { return !TaskQueue.empty() || !bIsRunning; });
			if (!bIsRunning && TaskQueue.empty())
			{
				return;
			}
			Task = std::move(TaskQueue.front());
			TaskQueue.pop();
		}

		if (Task)
		{
			Task();
		}
	}
}
