module;

#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <algorithm>

export module EventSystem;

import EventListener;
import EventBase;

struct ListenerInfo
{
	EventListener* Listener;
	int CategoryFilter;
	int Priority;
};

export class EventSystem
{
public:

	void SetImmediateMode(bool bNewMode)
	{
		bImmediateMode = bNewMode;
	}


	void AddListener(EventListener* Listener, int CategoryFilter = -1, int Priority = 0)
	{
		Listeners.push_back({Listener, CategoryFilter});

		// Sort listeners by priority (higher priority first)
		std::sort(Listeners.begin(), Listeners.end(),
			[](const ListenerInfo& A, const ListenerInfo& B)
			{
				return A.Priority > B.Priority;
			}
		);
	}

	bool RemoveListener(EventListener* Listener)
	{
		auto it = std::find(
			Listeners.begin(), Listeners.end(),
			[Listener](const ListenerInfo& Info) { return Info.Listener == Listener; }
		);
		
		if (it != Listeners.end())
		{
			Listeners.erase(it);
			return true;
		}
		return false;
	}

	void PublishEvent(const EventBase& Event)
	{
		if (bImmediateMode)
		{
			for (const ListenerInfo& Info: Listeners)
			{
				if (Info.CategoryFilter == -1 || (Event.GetCategoryFlags() & Info.CategoryFilter))
				{
					Info.Listener->OnEvent(Event);
				}
			}
		}
		else
		{
			std::lock_guard<std::mutex> Lock(QueueMutex);
			EventQueue.push(std::unique_ptr<EventBase>(Event.Clone()));
		}
	}

	void ProcessQueue()
	{
		if (bImmediateMode)
		{
			return;
		}

		std::queue<std::unique_ptr<EventBase>> CurrentEvents;

		{
			std::lock_guard<std::mutex> Lock(QueueMutex);
			std::swap(CurrentEvents, EventQueue);
		}

		while (!CurrentEvents.empty())
		{
			auto& Event = *CurrentEvents.front();

			for (const ListenerInfo& Info : Listeners)
			{
				if (Info.CategoryFilter == -1 || (Event.GetCategoryFlags() & Info.CategoryFilter))
				{
					Info.Listener->OnEvent(Event);
				}
			}

			CurrentEvents.pop();
		}
	}

private:
	std::vector<ListenerInfo> Listeners;
	std::queue<std::unique_ptr<EventBase>> EventQueue;
	std::mutex QueueMutex;
	bool bImmediateMode = true;
};

