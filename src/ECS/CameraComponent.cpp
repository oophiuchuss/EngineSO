module;

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>

module CameraComponent;

import Entity;
import TransformComponent;

bool Frustum::Intersects(const BoundingBox& Box) const
{
	for (auto& Plane : Planes)
	{
		glm::vec3 Normal = glm::vec3(Plane);
		float Distance = Plane.w;
		glm::vec3 PositiveVertex = Box.Min;
		
		if (Normal.x >= 0) PositiveVertex.x = Box.Max.x;
		if (Normal.y >= 0) PositiveVertex.y = Box.Max.y;
		if (Normal.z >= 0) PositiveVertex.z = Box.Max.z;
		if (glm::dot(Normal, PositiveVertex) + Distance < 0)
			return false; // Box is outside this plane
	}
	return true;
}

CameraComponent::CameraComponent(float InFieldOfView, float InAspectRatio, float InNearPlane, float InFarPlane)
{
	SetPerspective(InFieldOfView, InAspectRatio, InNearPlane, InFarPlane);
}

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

		ProjectionMatrix[1][1] *= -1.0f;  // flip Y for Vulkan NDC

		bProjectionDirty = false;
	}

	return ProjectionMatrix;
}

Frustum CameraComponent::GetFrustum() const
{
	glm::mat4 VP = GetProjectionMatrix() * GetViewMatrix();
	Frustum F;

	// Correctly extract rows from GLM column-major matrix
	auto GetRow = [&](int i) -> glm::vec4
		{
			return glm::vec4(VP[0][i], VP[1][i], VP[2][i], VP[3][i]);
		};

	glm::vec4 Row0 = GetRow(0);
	glm::vec4 Row1 = GetRow(1);
	glm::vec4 Row2 = GetRow(2);
	glm::vec4 Row3 = GetRow(3);

	F.Planes[0] = Row3 + Row0; // Left
	F.Planes[1] = Row3 - Row0; // Right
	F.Planes[2] = Row3 + Row1; // Bottom
	F.Planes[3] = Row3 - Row1; // Top
	F.Planes[4] = Row2;        // Near — GLM_FORCE_DEPTH_ZERO_TO_ONE: just row2
	F.Planes[5] = Row3 - Row2; // Far

	for (auto& Plane : F.Planes)
	{
		float Len = glm::length(glm::vec3(Plane));
		Plane /= Len;
	}

	return F;
}
