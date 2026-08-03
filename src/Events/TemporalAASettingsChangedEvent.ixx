module;

#include "EventMacros.h"

export module TemporalAASettingsChangedEvent;

import EventBase;
import TemporalAASettings;

export class TemporalAASettingsChangedEvent : public EventBase
{
public:
    DEFINE_EVENT_TYPE(TemporalAASettingsChangedEvent, static_cast<int>(EventCategory::Rendering))

    explicit TemporalAASettingsChangedEvent(const TemporalAASettings& InSettings): Settings(InSettings) {}

    const TemporalAASettings& GetSettings() const
    {
        return Settings;
    }

private:
    TemporalAASettings Settings;
};