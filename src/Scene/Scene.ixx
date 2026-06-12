module;

#include <glm/glm.hpp>
#include <string>
#include <memory>

export module Scene;

import SceneData;
import Entity;
import MeshData;
import Material;
import CameraComponent;
import ResourceHandle;
import EventSystem;
import ResourceManager;

export class Ray
{
public:
	glm::vec3 Origin;		// Starting point of the ray in world space
	glm::vec3 Direction;	// Normalized direction vector of the ray
	// TODO: implement ray
};

export class RayCastHit
{
public:
	glm::vec3 Point;		// Point of intersection in world space
	// TODO: implement ray
};

export class Scene
{
public:
	Scene(EventSystem* InEventSystem) : EventSystemPtr(InEventSystem) {}

	void Update(float DeltaTime);

	void InstantiateSceneFromData(SceneData& Data, ResourceManager& RM, const glm::mat4& RootTransform = glm::mat4(1.0f));

	// Entity factories
	Entity* CreateEntity(const std::string& Name);
	Entity* CreateCameraEntity(const std::string& Name, float FieldOfView = 60.0f, float AspectRatio = 16.0f / 9.0f, float NearPlane = 0.1f, float FarPlane = 1000.0f);
	Entity* CreateMeshEntity(const std::string& Name, ResourceHandle<MeshData> InMeshHandle, ResourceHandle<Material> InMaterialHandle);

	// Creates a child entity attached to the given parent
	Entity* CreateChildEntity(const std::string& Name, Entity* Parent);

	// Active camera management
	void SetActiveCameraEntity(Entity* CameraEntity);
	CameraComponent* GetActiveCameraComponent() const;

	// Queries
	inline std::vector<Entity*> GetAllEntities() const { return EntityList; }
	std::vector<Entity*> GetRenderableEntities() const;
	Entity* GetEntityByName(const std::string& Name) const;

	// Destruction
	void DestroyEntity(Entity* EntityToDestroy);

	bool Raycast(const Ray& InRay, RayCastHit& OutHit, float MaxDistance = 100.0f) const;

	// TODO Placeholder for scene data and methods
	// In a full implementation, this would include methods to query objects, perform raycasts, etc.

private:

	std::string GenerateUniqueEntityName(const std::string& BaseName) const;

	EventSystem* EventSystemPtr = nullptr;

	std::unordered_map<std::string, std::unique_ptr<Entity>> EntityMap;	// Primary storage of entities, keyed by unique name for easy lookup

	std::vector<Entity*> EntityList;			// Optional additional storage for iteration or other access patterns, if needed
	Entity* ActiveCameraEntity = nullptr;	// Pointer to the currently active camera entity, if any
};

