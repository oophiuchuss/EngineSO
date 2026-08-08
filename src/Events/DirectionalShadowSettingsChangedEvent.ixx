module;

#include "EventMacros.h"

export module DirectionalShadowSettingsChangedEvent;

import EventBase;
import DirectionalShadowSettings;

export class DirectionalShadowSettingsChangedEvent : public EventBase
{
public:
    DEFINE_EVENT_TYPE(DirectionalShadowSettingsChangedEvent, static_cast<int>(EventCategory::Rendering))

    explicit DirectionalShadowSettingsChangedEvent(const DirectionalShadowSettings& InSettings) : Settings(InSettings) {}

    const DirectionalShadowSettings& GetSettings() const
    {
        return Settings;
    }

private:
    DirectionalShadowSettings Settings;
};