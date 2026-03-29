module;

#include <glm/glm.hpp>

export module CameraComponent;

import Component;

export class CameraComponent : public ComponentBase
{
public:
	void SetPerspective(float InFieldOfView, float InAspectRatio, float InNearPlane, float InFarPlane);
    glm::mat4 GetViewMatrix() const;
	glm::mat4 GetProjectionMatrix() const;

private:
    float FieldOfView = 45.0f;
    float AspectRatio = 16.0f / 9.0f;
    float NearPlane = 0.1f;
    float FarPlane = 1000.0f;

	glm::mat4 ViewMatrix = glm::mat4(1.0f); // TODO: should be cached in the future if needed

    mutable glm::mat4 ProjectionMatrix = glm::mat4(1.0f);
    mutable bool bProjectionDirty = true;
};