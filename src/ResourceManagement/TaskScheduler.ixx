module;

#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <future>
#include <stdexcept>

export module TaskScheduler;

export class TaskScheduler
{
public:
	// Creates a pool with (hardware_concurrency - 1) threads (minimum 1)
	TaskScheduler();

	// Creates a pool with exactly `ThreadCount` threads
	explicit TaskScheduler(int ThreadCount);

	~TaskScheduler();

	// Submit a task to run on a background thread (returns immediately)
	void RunAsync(std::function<void()> Task);

	// Submit a task to run on the MAIN thread (call ProcessMainThreadTasks to execute)
	void RunOnMainThread(std::function<void()> Task);

	// Must be called on the main thread (e.g., in VulkanEngine::MainLoop)
	void ProcessMainThreadTasks();

	// Stop all workers and wait for them to finish
	void Stop();

	// Wait for all previously submitted async tasks to complete
	void WaitForAll();

private:
	void WorkerLoop(int WorkerIndex);

	std::vector<std::thread> Workers;
	std::queue<std::function<void()>> TaskQueue;
	std::mutex QueueMutex;
	std::condition_variable Condition;
	bool bIsRunning = true;

	int PendingCount = 0;
	std::mutex PendingMutex;
	std::condition_variable PendingCV;

	std::queue<std::function<void()>> MainThreadQueue;
	std::mutex MainThreadMutex;
};