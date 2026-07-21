module;

#include "EventMacros.h"

export module CursorCaptureRequestEvent;

import EventBase;

export class CursorCaptureRequestEvent : public EventBase
{
public:
    DEFINE_EVENT_TYPE(CursorCaptureRequestEvent, static_cast<int>(EventCategory::Window))

    explicit CursorCaptureRequestEvent(bool bInCaptured) : bCaptured(bInCaptured)
    {
    }

    inline bool ShouldCapture() const { return bCaptured; }

private:
    bool bCaptured = false;
};