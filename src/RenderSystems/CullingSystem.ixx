module;

#include <vector>

export module CullingSystem;

import Entity;
import MeshComponent;
import CameraComponent;

export class CullingSystem
{
public:
	explicit CullingSystem() : CameraComponentPtr(nullptr) {}

	void SetActiveCamera(CameraComponent* Camera) { CameraComponentPtr = Camera; }

	void CullScene(const std::vector<Entity*>& AllEntities);

	inline std::vector<Entity*> GetAllVisibleEntities() const { return VisibleEntities; };

	inline CameraComponent* GetActiveCamera() const { return CameraComponentPtr; }

private:
	CameraComponent* CameraComponentPtr;	
	std::vector<Entity*> VisibleEntities;
};