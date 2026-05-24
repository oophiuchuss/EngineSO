module;

#include <glm/glm.hpp>

export module Scene;

export class Ray
{
public:
	glm::vec3 Origin;		// Starting point of the ray in world space
	glm::vec3 Direction;	// Normalized direction vector of the ray
};

export class RayCastHit
{
public:
	glm::vec3 Point;		// Point of intersection in world space
};

export class Scene
{
public:

	bool Raycast(const Ray& InRay, RayCastHit& OutHit, float MaxDistance = 100.0f) const;

	// TODO Placeholder for scene data and methods
	// In a full implementation, this would include methods to query objects, perform raycasts, etc.
};

