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
		bProjectionDirty = false;
	}

	return ProjectionMatrix;
}

Frustum CameraComponent::GetFrustum() const
{
	glm::mat4 VP = GetProjectionMatrix() * GetViewMatrix();
	Frustum F;

	// Helper to extract column
	auto getRow = [&](int i) -> glm::vec4 {
		return glm::vec4(VP[i][0], VP[i][1], VP[i][2], VP[i][3]);
		};
	glm::vec4 row0 = getRow(0);
	glm::vec4 row1 = getRow(1);
	glm::vec4 row2 = getRow(2);
	glm::vec4 row3 = getRow(3);

	F.Planes[0] = row3 + row0; // Left
	F.Planes[1] = row3 - row0; // Right
	F.Planes[2] = row3 + row1; // Bottom
	F.Planes[3] = row3 - row1; // Top
	F.Planes[4] = row3 + row2; // Near
	F.Planes[5] = row3 - row2; // Far

	// Normalize planes
	for (auto& plane : F.Planes)
	{
		float len = glm::length(glm::vec3(plane));
		plane /= len;
	}
	return F;
}
