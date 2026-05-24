module;

#include <glm/glm.hpp>
#include <algorithm>

export module CameraComponent;

import Component;
import Geometry;

export struct Frustum
{
	glm::vec4 Planes[6]; 
	bool Intersects(const BoundingBox& Box) const;
};

export class CameraComponent : public ComponentBase
{
public:
	void SetPerspective(float InFieldOfView, float InAspectRatio, float InNearPlane, float InFarPlane);
    glm::mat4 GetViewMatrix() const;
	glm::mat4 GetProjectionMatrix() const;
	Frustum GetFrustum() const;

	inline float GetFieldOfView() const { return FieldOfView; }
	inline float GetAspectRatio() const { return AspectRatio; }
	inline float GetNearPlane() const { return NearPlane; }
	inline float GetFarPlane() const { return FarPlane; }

private:
    float FieldOfView = 45.0f;
    float AspectRatio = 16.0f / 9.0f;
    float NearPlane = 0.1f;
    float FarPlane = 1000.0f;

	glm::mat4 ViewMatrix = glm::mat4(1.0f); // TODO: should be cached in the future if needed

    mutable glm::mat4 ProjectionMatrix = glm::mat4(1.0f);
    mutable bool bProjectionDirty = true;
};

