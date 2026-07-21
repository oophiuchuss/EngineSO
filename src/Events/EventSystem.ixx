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
	EventSystem() = default;

	void SetImmediateMode(bool bNewMode)
	{
		bImmediateMode = bNewMode;
	}


	void AddListener(EventListener* Listener, int CategoryFilter = -1, int Priority = 0)
	{
		ListenerInfo Info({Listener, CategoryFilter, Priority});

		// Vector should be always sorted by priority, insert new listener in the correct position to maintain order
		auto It = std::lower_bound(
			Listeners.begin(), Listeners.end(), Info,
			[](const ListenerInfo& A, const ListenerInfo& B)
			{ 
				return A.Priority > B.Priority; 
			});

		Listeners.insert(It, Info);
	}

	bool RemoveListener(EventListener* Listener)
	{
		auto it = std::find_if(
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
			DispatchEvent(Event);
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
			DispatchEvent(*CurrentEvents.front());
			CurrentEvents.pop();
		}
	}

private:
	void DispatchEvent(const EventBase& Event)
	{
		for (const ListenerInfo& Info : Listeners)
		{
			if (Info.CategoryFilter == -1 || (Event.GetCategoryFlags() & Info.CategoryFilter))
			{
				if (Info.Listener->OnEvent(Event) == EventReply::Handled)
				{
					break;
				}
			}
		}
	}

	std::vector<ListenerInfo> Listeners;
	std::queue<std::unique_ptr<EventBase>> EventQueue;
	std::mutex QueueMutex;
	bool bImmediateMode = true;
};

