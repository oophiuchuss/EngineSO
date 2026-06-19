module;

#include <thread>
#include <mutex>
#include <condition_variable>

module TaskScheduler;

TaskScheduler::TaskScheduler()
{
    int NumThreads = static_cast<int>(std::thread::hardware_concurrency());
    if (NumThreads > 1) 
    {
        --NumThreads;   // leave one for the main/render thread
    }

    if (NumThreads < 1) 
    {
        NumThreads = 1;
    }

    Workers.reserve(NumThreads);
    for (int i = 0; i < NumThreads; ++i)
    {
        Workers.emplace_back(&TaskScheduler::WorkerLoop, this, i);
    }
}

TaskScheduler::TaskScheduler(int ThreadCount)
{
    if (ThreadCount < 1) 
    {
        ThreadCount = 1;
    }

    Workers.reserve(ThreadCount);
    for (int i = 0; i < ThreadCount; ++i)
    {
        Workers.emplace_back(&TaskScheduler::WorkerLoop, this, i);
    }
}

TaskScheduler::~TaskScheduler()
{
    Stop();
}

void TaskScheduler::RunAsync(std::function<void()> Task)
{
    {
        std::lock_guard<std::mutex> Lock(QueueMutex);
        TaskQueue.push(std::move(Task));
        ++PendingCount;
    }

    Condition.notify_one();
}

void TaskScheduler::RunOnMainThread(std::function<void()> Task)
{
    std::lock_guard<std::mutex> Lock(MainThreadMutex);
    MainThreadQueue.push(std::move(Task));
}

void TaskScheduler::ProcessMainThreadTasks()
{
    std::queue<std::function<void()>> LocalQueue;
    {
        std::lock_guard<std::mutex> Lock(MainThreadMutex);
        std::swap(LocalQueue, MainThreadQueue);
    }

    while (!LocalQueue.empty())
    {
        LocalQueue.front()();
        LocalQueue.pop();
    }
}

void TaskScheduler::Stop()
{
    {
        std::lock_guard<std::mutex> Lock(QueueMutex);
        bIsRunning = false;
    }
    Condition.notify_all();

    for (auto& Worker : Workers)
    {
        if (Worker.joinable())
        {
            Worker.join();
        }
    }
    Workers.clear();
}

void TaskScheduler::WaitForAll()
{
    std::unique_lock<std::mutex> Lock(PendingMutex);
    PendingCV.wait(Lock, [this]() { return PendingCount == 0; });
}

void TaskScheduler::WorkerLoop(int WorkerIndex)
{
    while (true)
    {
        std::function<void()> Task;
        {
            std::unique_lock<std::mutex> Lock(QueueMutex);
            Condition.wait(Lock, [this]() { return !TaskQueue.empty() || !bIsRunning; });

            if (!bIsRunning && TaskQueue.empty())
                return;

            Task = std::move(TaskQueue.front());
            TaskQueue.pop();
        }

        if (Task)
        {
            try { Task(); }
            catch (const std::exception& e) { /* log / store error */ }
            catch (...) { /* ignore unknown errors */ }
        }

        {
            std::lock_guard<std::mutex> Lock(PendingMutex);
            --PendingCount;
        }
        PendingCV.notify_one();
    }
}
