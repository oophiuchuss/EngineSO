module;

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

module CameraComponent;

import Entity;
import TransformComponent;

void CameraComponent::SetPerspective(float InFieldOfView, float InAspectRatio, float InNearPlane, float InFarPlane)
{
	FieldOfView = InFieldOfView;
	AspectRatio = InAspectRatio;
	NearPlane = InNearPlane;
	FarPlane = InFarPlane;
	bProjectionDirty = true;
}

glm::mat4 CameraComponent::GetViewMatrix() const
{
	auto TransformComp = GetOwner()->GetComponent<TransformComponent>();
	if (TransformComp)
	{
		glm::vec3 Position = TransformComp->GetPosition();
		glm::quat Rotation = TransformComp->GetRotation();

		glm::vec3 Forward = Rotation * glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 Up = Rotation * glm::vec3(0.0f, 1.0f, 0.0f);

		return glm::lookAt(Position, Position + Forward, Up);
	}
	
	return glm::mat4(1.0f);
}

glm::mat4 CameraComponent::GetProjectionMatrix() const
{
	if (bProjectionDirty)
	{
		ProjectionMatrix = glm::perspective(
			glm::radians(FieldOfView),
			AspectRatio,
			NearPlane,
			FarPlane
		);
		bProjectionDirty = false;
	}

	return ProjectionMatrix;
}