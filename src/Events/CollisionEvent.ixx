export module CollisionEvent;

import EventBase;
import Entity;

export class CollisionEvent : public EventBase
{
public:
	CollisionEvent(Entity* InEntityA, Entity* InEntityB);
	inline Entity* GetEntityA() const { return EntityA; }
	inline Entity* GetEntityB() const { return EntityB; }

private:
	Entity* EntityA;
	Entity* EntityB;
};