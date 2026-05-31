module;

#include <vector>

module CullingSystem;

import Entity;
import MeshComponent;
import TransformComponent;

void CullingSystem::CullScene(const std::vector<Entity*>& AllEntities)
{
	VisibleEntities.clear();
    if (!CameraComponentPtr)
    {
		return; // No active camera, cannot perform culling
    }

    Frustum CurFrustum = CameraComponentPtr->GetFrustum();

    for (auto* Entity : AllEntities)
    {
        if (!Entity || !Entity->IsActive())
            continue;

        MeshComponent* Mesh = Entity->GetComponent<MeshComponent>();
        TransformComponent* Transform = Entity->GetComponent<TransformComponent>();

        if (Mesh && Transform)
        {
            BoundingBox WorldBox = Mesh->GetMeshData()->GetBoundingBox();
            WorldBox.Transform(Transform->GetTransformMatrix());

            if(CurFrustum.Intersects(WorldBox))
            {
                VisibleEntities.push_back(Entity);
            }
        }
    }
}