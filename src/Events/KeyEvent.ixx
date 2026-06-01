module;

#include "EventMacros.h"	

export module KeyEvent;

import EventBase;

export class KeyEvent : public EventBase
{
public:
	DEFINE_EVENT_TYPE(KeyEvent, static_cast<int>(EventCategory::Input) | static_cast<int>(EventCategory::Keyboard))

	KeyEvent(int Key, KeyAction Action) : KeyCode(Key), Action(Action){}

	int GetKeyCode() const { return KeyCode; }
	KeyAction GetAction() const { return Action; }

private:
	int KeyCode = 0;
	KeyAction Action = KeyAction::Press;
};