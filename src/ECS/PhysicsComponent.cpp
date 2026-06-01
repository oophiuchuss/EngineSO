module PhysicsComponent;

import CollisionEvent;

void PhysicsComponent::OnInitialize()
{
	EventSystem::Get().AddListener(this);
}

PhysicsComponent::~PhysicsComponent()
{
	EventSystem::Get().RemoveListener(this);
}

void PhysicsComponent::OnEvent(const EventBase& Event)
{
	if (const auto CollisionEventPtr = dynamic_cast<const CollisionEvent*>(&Event))
	{
		// Handle collision event
		Entity* EntityA = CollisionEventPtr->GetEntityA();
		Entity* EntityB = CollisionEventPtr->GetEntityB();
		
		// TODO Implement physics response to the collision between EntityA and EntityB
	}
}
