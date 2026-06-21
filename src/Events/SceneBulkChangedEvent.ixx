module;

#include "EventMacros.h"	

export module SceneBulkChangedEvent;

import EventBase;
import Scene;

export class SceneBulkChangedEvent : public EventBase
{
public:
	DEFINE_EVENT_TYPE(SceneBulkChangedEvent, static_cast<int>(EventCategory::Scene))

	SceneBulkChangedEvent(Scene* InScenePtr) : ScenePtr(InScenePtr) {}


	Scene* GetScene() const { return ScenePtr; }

private:
	Scene* ScenePtr = nullptr;
};