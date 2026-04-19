module;

#include "EventMacros.h"	

export module WindowResizeEvent;

import EventBase;

export class WindowResizeEvent : public EventBase
{
public:
	DEFINE_EVENT_TYPE(WindowResizeEvent, static_cast<int>(EventCategory::Application) | static_cast<int>(EventCategory::Window))

	WindowResizeEvent(int W, int H) : Width(W), Height(H) {}

	int GetWidth() const { return Width; }
	int GetHeight() const { return Height; }


private:
	int Width = 0;
	int Height = 0;
};