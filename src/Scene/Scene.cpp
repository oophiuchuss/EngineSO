module;

#include <glm/glm.hpp>
#include <string>
#include <memory>

module Scene;

import Entity;
import MeshData;
import ShaderData;
import CameraComponent;
import TransformComponent;
import MeshComponent;
import ResourceManager;

void Scene::Update(float DeltaTime)
{
	for (Entity* CurEntity : EntityList)
	{
		CurEntity->Update(DeltaTime);
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

Entity* Scene::CreateMeshEntity(const std::string& Name, ResourceHandle<MeshData> InMeshHandle, ResourceHandle<ShaderData> InShaderHandle)
{
	Entity* MeshEntity = CreateEntity(Name);
	MeshEntity->AddComponent<TransformComponent>();
	MeshEntity->AddComponent<MeshComponent>(InMeshHandle, InShaderHandle);
	
	return MeshEntity;
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