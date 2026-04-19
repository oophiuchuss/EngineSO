export module EventDispatcher;

import EventBase;

export class EventDispatcher
{
public:
	explicit EventDispatcher(const EventBase& E) : Event(E) {};

	// Dispatch event to handler if exists
	template<typename T, typename F>
	bool Dispatch(const F& Handler)
	{
		if (Event.GetType() == T::GetStaticType())
		{
			Handler(static_cast<const T&>(Event));
			return true;
		}
		
		return false;
	}

private:
	const EventBase& Event;
};