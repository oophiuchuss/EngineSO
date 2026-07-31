module;

#include "EventMacros.h"

export module RenderDebugSettingsChangedEvent;

import EventBase;
import RenderDebugSettings;

export class RenderDebugSettingsChangedEvent : public EventBase
{
public:
    DEFINE_EVENT_TYPE(RenderDebugSettingsChangedEvent, static_cast<int>(EventCategory::Rendering))

    explicit RenderDebugSettingsChangedEvent(const RenderDebugSettings& InSettings) : Settings(InSettings) {}

    const RenderDebugSettings& GetSettings() const
    {
        return Settings;
    }

private:
    RenderDebugSettings Settings;
};