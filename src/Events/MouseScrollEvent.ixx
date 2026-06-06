module;

#include "EventMacros.h"

export module MouseScrollEvent;

import EventBase;

export class MouseScrollEvent : public EventBase
{
public:
    DEFINE_EVENT_TYPE(MouseScrollEvent,
        static_cast<int>(EventCategory::Input) |
        static_cast<int>(EventCategory::Mouse))

    MouseScrollEvent(double InXOffset, double InYOffset)
    : XOffset(InXOffset), YOffset(InYOffset) {}

    double GetXOffset() const { return XOffset; }
    double GetYOffset() const { return YOffset; }

private:
    double XOffset = 0.0;
    double YOffset = 0.0;

};