module MeshComponent;

import Entity;
import TransformComponent;


void MeshComponent::Render()
{
	if (!MeshData || !MaterialData)
	{
		return;
	}

	auto TransformComp = GetOwner()->GetComponent<TransformComponent>();

	if (!TransformComp)
	{
		return;
	}

	//TODO: render material and mesh
}

BoundingBox MeshComponent::GetBoundingBox() const
{
	if (MeshData)
	{
		return MeshData->GetBoundingBox();
	}

	return BoundingBox();
}
