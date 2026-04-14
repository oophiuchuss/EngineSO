module;

#include <vector>

module CullingSystem;

import Entity;
import MeshComponent;
import TransformComponent;

void CullingSystem::CullScene(const std::vector<Entity*>& AllEntitites)
{
	VisibleEntities.clear();

	if (!CameraPtr)
	{
		return; // No camera, can't cull
	}

	Frustum CameraFrustum = CameraPtr->GetFrustum();

	for (auto* CurEntity : AllEntitites)
	{
		if (IsEntityVisible(CurEntity, CameraFrustum))
		{
			VisibleEntities.push_back(CurEntity);
		}
	}
}

bool CullingSystem::IsEntityVisible(const Entity* InEntity, const Frustum& CameraFrustum) const
{
	if (InEntity == nullptr)
	{
		return false;
	}

	if (!InEntity->IsActive())
	{
		return false;
	}

	MeshComponent* MeshComp = InEntity->GetComponent<MeshComponent>();
	if (!MeshComp)
	{
		return false;
	}

	TransformComponent* TransformComp = InEntity->GetComponent<TransformComponent>();
	if (!TransformComp)
	{
		return false;
	}

	BoundingBox EntityBoundingBox = MeshComp->GetBoundingBox();

	EntityBoundingBox.Transform(TransformComp->GetTransformMatrix());

	if (!CameraFrustum.Intersects(EntityBoundingBox))
	{
		return false;
	}

	return true;
}