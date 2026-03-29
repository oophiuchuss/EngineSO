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