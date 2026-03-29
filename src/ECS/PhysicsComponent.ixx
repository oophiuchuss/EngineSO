export module PhysicsComponent;

import Component;
import EventBase;
import EventSystem;
import EventListener;

export class PhysicsComponent : public ComponentBase, public EventListener
{
public:
	void Initialize() override;
	
	~PhysicsComponent() override;

	void OnEvent(const EventBase& Event) override;

private:
	EventSystem* GetEventSystem() const;
};