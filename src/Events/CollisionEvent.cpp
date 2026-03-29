module CollisionEvent;

import Entity;

CollisionEvent::CollisionEvent(Entity* InEntityA, Entity* InEntityB) : EntityA(InEntityA), EntityB(InEntityB)
{
}