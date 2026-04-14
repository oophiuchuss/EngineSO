module;

#include <vector>

export module CullingSystem;

import Entity;
import MeshComponent;

// TODO: Implement the Camera and Frustrum class and its methods
export class Frustum
{
public:
	bool Intersects(const BoundingBox& Box) const
	{
		return true; // Placeholder implementation
	}
};
export class Camera
{
public:
	Frustum GetFrustum() const { return Frustum(); } // Placeholder implementation
};


export class CullingSystem
{
public:
	explicit CullingSystem(Camera* camera) : CameraPtr(camera) {}

	void SetCamera(Camera* camera) { CameraPtr = camera; }

	void CullScene(const std::vector<Entity*>& AllEntitites);

	bool IsEntityVisible(const Entity* InEntity, const Frustum& CameraFrustum) const;
	
	inline std::vector<Entity*> GetAllVisibleEntities() const { return VisibleEntities; };

private:
	Camera* CameraPtr;	
	std::vector<Entity*> VisibleEntities;
};