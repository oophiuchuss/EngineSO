module;

#include <type_traits>
#include <algorithm>

module Entity;

import Component;

void Entity::SetParent(Entity* InParent)
{
	// Remove from old parent if any
	if (Parent)
	{
		Parent->RemoveChild(this);
	}

	Parent = InParent;
}

void Entity::AddChild(Entity* InChild)
{
	if (!InChild)
	{
		return;
	}

	// Can't parent to itself
	if (InChild == this)
	{
		return;
	}

	// Check if InChild is already an ancestor – would create a cycle
	for (Entity* Ancestor = this; Ancestor != nullptr; Ancestor = Ancestor->GetParent())
	{
		if (Ancestor == InChild)
		{
			return;   // cycle detected, ignore
		}
	}

	auto It = std::find(Children.begin(), Children.end(), InChild);
	if (It != Children.end())
	{
		return;
	}

	Children.push_back(InChild);
	InChild->SetParent(this);
}

void Entity::RemoveChild(Entity* InChild)
{
	if (!InChild)
	{
		return;
	}

	auto It = std::find(Children.begin(), Children.end(), InChild);
	if (It != Children.end())
	{
		(*It)->Parent = nullptr;  // clear child's parent pointer
		Children.erase(It);
	}
}

void Entity::Initialize()
{
	for (std::unique_ptr<ComponentBase>& CurComponent : Components)
	{
		CurComponent->Initialize();
	}

	SetActive(true);

	// Initialize children too — ensures full hierarchy is ready
	for (Entity* Child : Children)
	{
		Child->Initialize();
	}
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

	// Update children after parent — ensures parent transform is up to date first
	for (Entity* Child : Children)
	{
		Child->Update(DeltaTime);
	}
}
