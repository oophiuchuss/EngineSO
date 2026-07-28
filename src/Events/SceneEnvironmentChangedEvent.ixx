module;

#include "EventMacros.h"

export module SceneEnvironmentChangedEvent;

import EventBase;
import Scene;

export class SceneEnvironmentChangedEvent : public EventBase
{
public:
    DEFINE_EVENT_TYPE(SceneEnvironmentChangedEvent, static_cast<int>(EventCategory::Scene))

    explicit SceneEnvironmentChangedEvent(Scene* InScenePtr) : ScenePtr(InScenePtr) {}

    Scene* GetScene() const
    {
        return ScenePtr;
    }

private:
    Scene* ScenePtr = nullptr;
};