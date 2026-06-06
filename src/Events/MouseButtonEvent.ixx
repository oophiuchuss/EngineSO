module;

#include "EventMacros.h"

export module MouseButtonEvent;

import EventBase;

export enum class MouseButtonAction
{
    Press,
    Release
};

export class MouseButtonEvent : public EventBase
{
public:
    DEFINE_EVENT_TYPE(MouseButtonEvent,
        static_cast<int>(EventCategory::Input) |
        static_cast<int>(EventCategory::MouseButton))   // uses existing enum

    MouseButtonEvent(int Button, MouseButtonAction Action)
        : Button(Button), Action(Action) {}



    int                GetButton() const { return Button; }
    MouseButtonAction  GetAction() const { return Action; }

private:
    int               Button = 0;
    MouseButtonAction Action = MouseButtonAction::Press;
};