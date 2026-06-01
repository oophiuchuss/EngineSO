module;

#include <type_traits>

module Entity;

import Component;

void Entity::Initialize()
{
	for (std::unique_ptr<ComponentBase>& CurComponent : Components)
	{
		CurComponent->Initialize();
	}

	SetActive(true);
}

void Entity::Update(float DeltaTime)
{
	if (!bIsActive)
	{
		return;
	}

	for (std::unique_ptr<ComponentBase>& CurComponent : Components)
	{
		if (CurComponent->GetComponentState() == ComponentState::Active)
		{
			CurComponent->Update(DeltaTime);
		}
	}
}

void Entity::Render()
{
	if (!bIsActive)
	{
		return;
	}

	for (std::unique_ptr<ComponentBase>& CurComponent : Components)
	{
		CurComponent->Render();
	}
}
