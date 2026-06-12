module;

#include <glm/glm.hpp>
#include <string>
#include <memory>

module Scene;

import Entity;
import MeshData;
import Material;
import CameraComponent;
import TransformComponent;
import MeshComponent;
import ResourceManager;

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

	const auto& Entries = Data.GetMeshEntries();
	if (Entries.empty())
	{
		return;
	}

	// Create all entities flat, store entry index -> entity mapping
	std::vector<Entity*> EntryToEntity(Entries.size(), nullptr);

	for (size_t i = 0; i < Entries.size(); ++i)
	{
		const SceneMeshEntry& Entry = Entries[i];

		Entity* E = CreateEntity(Entry.Name);

		// Apply root transform to root entries — children inherit via hierarchy
		glm::mat4 FinalLocalTransform = RootTransform * Entry.LocalTransform;
		if (Entry.ParentIndex.has_value())
		{
			FinalLocalTransform = Entry.LocalTransform;
		}

		// Add transform component with local transform
		auto* TC = E->AddComponent<TransformComponent>();
		TC->SetTransformFromMatrix(FinalLocalTransform);

		// Get handles directly from ResourceManager via Scene's reference
		ResourceHandle<MeshData> MH = RM.GetHandle<MeshData>(Entry.MeshID);
		ResourceHandle<Material> MatH = RM.GetHandle<Material>(Entry.MaterialID);

		E->AddComponent<MeshComponent>(MH, MatH);

		EntryToEntity[i] = E;
	}

	// Wire parent/child relationships
	for (size_t i = 0; i < Entries.size(); ++i)
	{
		const SceneMeshEntry& Entry = Entries[i];
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
}

Entity* Scene::CreateEntity(const std::string& Name)
{
	std::string UniqueName = GenerateUniqueEntityName(Name);

	auto NewEntity = std::unique_ptr<Entity>(new Entity(UniqueName));
	Entity* NewEntityPtr = NewEntity.get();
	NewEntityPtr->Initialize();

	EntityMap.emplace(UniqueName, std::move(NewEntity));
	EntityList.push_back(NewEntityPtr);

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