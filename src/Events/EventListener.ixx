export module EventListener;

import EventBase;

export class EventListener
{
public:
	virtual ~EventListener() = default;
	virtual EventReply OnEvent(const EventBase& Event) = 0;
};
