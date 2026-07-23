module;

#include "EventMacros.h"

export module PostProcessSettingsChangedEvent;

import EventBase;
import PostProcessSettings;

export class PostProcessSettingsChangedEvent : public EventBase
{
public:
    DEFINE_EVENT_TYPE(PostProcessSettingsChangedEvent, static_cast<int>(EventCategory::Rendering))

    explicit PostProcessSettingsChangedEvent(const PostProcessSettings& InSettings) : Settings(InSettings)
    {
    }

    const PostProcessSettings& GetSettings() const
    {
        return Settings;
    }

private:
    PostProcessSettings Settings;
};