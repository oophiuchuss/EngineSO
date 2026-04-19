module;

#include "EventMacros.h"	

export module KeyEvent;

import EventBase;

export class KeyEvent : public EventBase
{
public:
	DEFINE_EVENT_TYPE(KeyEvent, static_cast<int>(EventCategory::Input) | static_cast<int>(EventCategory::Keyboard))

	KeyEvent(int Key, bool bRepeat) : KeyCode(Key), bIsRepeat(bRepeat) {}

	int GetKeyCode() const { return KeyCode; }
	bool IsRepeat() const { return bIsRepeat; }

private:
	int KeyCode = 0;
	bool bIsRepeat = false;
};