export module PhysicsComponent;

import Component;
import EventBase;
import EventSystem;
import EventListener;

export class PhysicsComponent : public ComponentBase, public EventListener
{
public:
	~PhysicsComponent() override;
	EventReply OnEvent(const EventBase& Event) override;

protected:
	void OnInitialize() override;
};