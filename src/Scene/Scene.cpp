module;

#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <iostream>

module Scene;

import Entity;
import MeshData;
import Material;
import CameraComponent;
import TransformComponent;
import MeshComponent;
import DirectionalLightComponent;
import PointLightComponent;
import SpotLightComponent;
import ResourceManager;

import EventSystem;
import SceneBulkChangedEvent;
import SceneEntityChangedEvent;

void Scene::Update(float DeltaTime)
{
	for (Entity* CurEntity : EntityList)
	{
		// Update only parent-less entities
		if (CurEntity->GetParent() == nullptr)
		{
			CurEntity->Update(DeltaTime);
		}
	}
}

void Scene::InstantiateSceneFromData(SceneData& Data, ResourceManager& RM, const glm::mat4& RootTransform)
{
	if (!Data.IsInstantiated())
	{
		Data.Instantiate();
	}

	const auto& Nodes = Data.GetNodeEntries();
	const auto& Meshes = Data.GetMeshInstances();
	const auto& Lights = Data.GetLightInstances();

	if (Nodes.empty())
	{
		return;
	}

	// Create all entities flat, store entry index -> entity mapping
	std::vector<Entity*> EntryToEntity(Nodes.size(), nullptr);

	for (size_t i = 0; i < Nodes.size(); i++)
	{
		const SceneNodeEntry& Entry = Nodes[i];

		Entity* E = CreateEntity(Entry.Name);

		// Apply root transform to root Nodes — children inherit via hierarchy
		glm::mat4 FinalLocalTransform = Entry.ParentIndex.has_value()
			? Entry.LocalTransform
			: RootTransform * Entry.LocalTransform;

		// Add transform component with local transform
		auto* TC = E->AddComponent<TransformComponent>();
		TC->SetTransformFromMatrix(FinalLocalTransform);

		EntryToEntity[i] = E;
	}


	// Attach meshes
	for (const MeshInstance& MI : Meshes)
	{
		if (MI.NodeIndex < 0 || MI.NodeIndex >= static_cast<int>(EntryToEntity.size()))
		{
			continue;
		}

		Entity* E = EntryToEntity[MI.NodeIndex];
		if (!E)
		{
			continue;
		}

		auto MeshHandle = RM.GetHandle<MeshData>(MI.MeshID);
		auto MatHandle = RM.GetHandle<Material>(MI.MaterialID);
		E->AddComponent<MeshComponent>(MeshHandle, MatHandle);
	}

	// Attach lights
	for (const LightInstance& LI : Lights)
	{
		if (LI.NodeIndex < 0 || LI.NodeIndex >= static_cast<int>(EntryToEntity.size()))
		{
			continue;
		}
		
		Entity* E = EntryToEntity[LI.NodeIndex];
		if (!E)
		{
			continue;
		}

		switch (LI.Type)
		{
		case LightType::Directional:
		{
			auto* Dir = E->AddComponent<DirectionalLightComponent>();
			Dir->SetColor(LI.Color);
			Dir->SetIntensity(LI.Intensity);
			break;
		}
		case LightType::Point:
		{
			auto* Pt = E->AddComponent<PointLightComponent>();
			Pt->SetColor(LI.Color);
			Pt->SetIntensity(LI.Intensity);
			Pt->SetRange(LI.Range);
			break;
		}
		case LightType::Spot:
		{
			auto* Sp = E->AddComponent<SpotLightComponent>();
			Sp->SetColor(LI.Color);
			Sp->SetIntensity(LI.Intensity);
			Sp->SetRange(LI.Range);
			Sp->SetInnerConeAngle(LI.InnerConeAngle);
			Sp->SetOuterConeAngle(LI.OuterConeAngle);
			break;
		}
		}
	}

	// Wire parent/child relationships
	for (size_t i = 0; i < Nodes.size(); i++)
	{
		const SceneNodeEntry& Entry = Nodes[i];
		Entity* E = EntryToEntity[i];

		if (Entry.ParentIndex.has_value())
		{
			int ParentIdx = Entry.ParentIndex.value();
			if (ParentIdx >= 0 && ParentIdx < static_cast<int>(EntryToEntity.size()))
			{
				Entity* Parent = EntryToEntity[ParentIdx];
				if (Parent)
				{
					Parent->AddChild(E);
				}
			}
		}
	}

	if (EventSystemPtr)
	{
		EventSystemPtr->PublishEvent(SceneBulkChangedEvent(this));
	}
}

Entity* Scene::CreateEntity(const std::string& Name)
{
	std::string UniqueName = GenerateUniqueEntityName(Name);

	auto NewEntity = std::unique_ptr<Entity>(new Entity(UniqueName));
	Entity* NewEntityPtr = NewEntity.get();
	NewEntityPtr->Initialize();

	EntityMap.emplace(UniqueName, std::move(NewEntity));
	EntityList.push_back(NewEntityPtr);

	if (EventSystemPtr)
	{
		// TODO: publish from somewhere else, since scene doesn't now whether entity changed mesh or texture
		EventSystemPtr->PublishEvent(SceneEntityChangedEvent(NewEntityPtr));
	}

	return NewEntityPtr;
}

Entity* Scene::CreateCameraEntity(const std::string& Name, float FieldOfView, float AspectRatio, float NearPlane, float FarPlane)
{
	Entity* CameraEntity = CreateEntity(Name);
	CameraEntity->AddComponent<TransformComponent>();
	CameraEntity->AddComponent<CameraComponent>(FieldOfView, AspectRatio, NearPlane, FarPlane);

	return CameraEntity;
}

Entity* Scene::CreateMeshEntity(const std::string& Name, ResourceHandle<MeshData> InMeshHandle, ResourceHandle<Material> InMaterialHandle)
{
	Entity* MeshEntity = CreateEntity(Name);
	MeshEntity->AddComponent<TransformComponent>();
	MeshEntity->AddComponent<MeshComponent>(InMeshHandle, InMaterialHandle);
	
	return MeshEntity;
}

Entity* Scene::CreateChildEntity(const std::string& Name, Entity* Parent)
{
	if (!Parent)
	{
		return nullptr;
	}

	Entity* Child = CreateEntity(Name);
	Parent->AddChild(Child);   // sets both parent and child links
	return Child;
}

void Scene::SetActiveCameraEntity(Entity* CameraEntity)
{
	if (!CameraEntity)
	{
		return;
	}

	if (!CameraEntity->IsActive())
	{
		return; // Inactive entity, ignore
	}

	CameraComponent* CamComponent = CameraEntity->GetComponent<CameraComponent>();

	if (!CamComponent)
	{
		return; // Not a camera entity, ignore
	}

	ActiveCameraEntity = CameraEntity;
}

CameraComponent* Scene::GetActiveCameraComponent() const
{
	if (ActiveCameraEntity && ActiveCameraEntity->IsActive())
	{
		return ActiveCameraEntity->GetComponent<CameraComponent>();
	}

	return nullptr;
}

std::vector<Entity*> Scene::GetRenderableEntities() const
{
	std::vector<Entity*> RenderableEntities;

	for (Entity* CurEntity : EntityList)
	{
		if (!CurEntity->IsActive())
		{
			continue;
		}

		if (CurEntity->GetComponent<MeshComponent>() && CurEntity->GetComponent<TransformComponent>() )
		{
			RenderableEntities.push_back(CurEntity);
		}
	}

	return RenderableEntities;
}

Entity* Scene::GetEntityByName(const std::string& Name) const
{
	auto it = EntityMap.find(Name);
	if (it != EntityMap.end())
	{
		return it->second.get();
	}

	return nullptr;
}

void Scene::DestroyEntity(Entity* EntityToDestroy)
{
	if (!EntityToDestroy)
	{
		return;
	}

	// Clear active camera reference if it's the one being destroyed
	if (EntityToDestroy == ActiveCameraEntity)
	{
		ActiveCameraEntity = nullptr;
	}

	// Destroy children recursively
	auto ChildrenCopy = EntityToDestroy->GetChildren();
	for (Entity* Child : ChildrenCopy)
	{
		DestroyEntity(Child);
	}

	// Remove from parent's child list if this entity is a child
	if (EntityToDestroy->GetParent())
	{
		EntityToDestroy->GetParent()->RemoveChild(EntityToDestroy);
	}

	// Remove from active camera if this entity held it
	if (EntityToDestroy == ActiveCameraEntity)
	{
		ActiveCameraEntity = nullptr;
	}

	std::string EntityName = EntityToDestroy->GetName();

	// Remove firstly from the list to avoid dangling pointers
	auto it = std::find(EntityList.begin(), EntityList.end(), EntityToDestroy);
	if (it != EntityList.end())
	{
		EntityList.erase(it);
	}

	EntityMap.erase(EntityName); // Unique pointer will automatically clean up the entity
}

std::string Scene::GenerateUniqueEntityName(const std::string& BaseName) const
{
	if (EntityMap.find(BaseName) == EntityMap.end())
	{
		return BaseName; // Base name is unique, return it as is
	}

	std::string NewName;

	for (int Counter = 0; ; Counter++)
	{
		NewName = BaseName + "_" + std::to_string(Counter);
		if (EntityMap.find(NewName) == EntityMap.end())
		{
			return NewName; // Found a unique name, return it
		}
	}

	return std::string();
}

bool Scene::Raycast(const Ray& InRay, RayCastHit& OutHit, float MaxDistance) const
{
	// TODO Placeholder for raycasting logic
	// In a full implementation, this would check for intersections with objects in the scene and populate OutHit with details of the hit.
	return false; // Return true if an intersection is detected, false otherwise
}