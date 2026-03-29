module;

#include <vector>

export module EventSystem;

import EventListener;
import EventBase;

export class EventSystem
{
public:
	void AddListener(EventListener* Listener)
	{
		Listeners.push_back(Listener);
	}

	bool RemoveListener(EventListener* Listener)
	{
		auto it = std::find(Listeners.begin(), Listeners.end(), Listener);
		if (it != Listeners.end())
		{
			Listeners.erase(it);
			return true;
		}
		return false;
	}

	void DispatchEvent(const EventBase& Event)
	{
		for (EventListener* Listener : Listeners)
		{
			if (Listener)
			{
				Listener->OnEvent(Event);
			}
		}
	}

private:
	std::vector<EventListener*> Listeners;
};

