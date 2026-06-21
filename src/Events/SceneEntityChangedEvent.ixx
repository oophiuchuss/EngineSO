module;

#include "EventMacros.h"	

export module SceneEntityChangedEvent;

import EventBase;
import Entity;

export class SceneEntityChangedEvent : public EventBase
{
public:
	DEFINE_EVENT_TYPE(SceneEntityChangedEvent, static_cast<int>(EventCategory::Scene));

	SceneEntityChangedEvent(Entity* InEntityPtr) : EntityPtr(InEntityPtr) {}

	Entity* GetEntity() const { return EntityPtr; }

private:
	Entity* EntityPtr = nullptr;
};