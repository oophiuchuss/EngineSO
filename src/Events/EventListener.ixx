export module EventListener;

import EventBase;

export class EventListener
{
public:
	virtual ~EventListener() = default;
	virtual void OnEvent(const EventBase& Event) = 0;
};
