module;

#include "EventMacros.h"

export module MouseMovedEvent;

import EventBase;

export class MouseMovedEvent : public EventBase
{
public:
    DEFINE_EVENT_TYPE(MouseMovedEvent,
        static_cast<int>(EventCategory::Input) |
        static_cast<int>(EventCategory::Mouse))

        MouseMovedEvent(double XOffset, double YOffset)
        : XOffset(XOffset), YOffset(YOffset) {
    }

    double GetXOffset() const { return XOffset; }
    double GetYOffset() const { return YOffset; }

private:
    double XOffset = 0.0;
    double YOffset = 0.0;
};