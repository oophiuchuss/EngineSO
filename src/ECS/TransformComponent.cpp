module;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

module TransformComponent;

import Entity;

void TransformComponent::SetTransformFromMatrix(const glm::mat4& Transform)
{
	glm::vec3 SkewUnused;
	glm::vec4 PerspectiveUnused;

	glm::decompose(
		Transform,
		Scale,
		Rotation,
		Position,
		SkewUnused,
		PerspectiveUnused);

	// glm::decompose returns conjugate quaternion
	Rotation = glm::conjugate(Rotation);
	bLocalDirty = true;
}

void TransformComponent::SetPosition(const glm::vec3& InPosition)
{
	Position = InPosition;
	bLocalDirty = true;
}

void TransformComponent::SetRotation(const glm::quat& InRotation)
{
	Rotation = InRotation;
	bLocalDirty = true;
}

void TransformComponent::SetScale(const glm::vec3& InScale)
{
	Scale = InScale;
	bLocalDirty = true;
}

glm::vec3 TransformComponent::GetWorldPosition() const
{
	// Column 3 of world matrix is the translation
	return glm::vec3(GetWorldTransformMatrix()[3]);
}

glm::vec3 TransformComponent::GetWorldScale() const
{
	glm::mat4 World = GetWorldTransformMatrix();

	// Length of each column vector gives the scale on that axis
	return glm::vec3(
		glm::length(glm::vec3(World[0])),
		glm::length(glm::vec3(World[1])),
		glm::length(glm::vec3(World[2])));
}

glm::quat TransformComponent::GetWorldRotation() const
{
	glm::mat4 World = GetWorldTransformMatrix();
	glm::vec3 S = GetWorldScale();

	// Remove scale from rotation columns to get pure rotation matrix
	glm::mat3 RotMat(
		glm::vec3(World[0]) / S.x,
		glm::vec3(World[1]) / S.y,
		glm::vec3(World[2]) / S.z);

	return glm::quat_cast(RotMat);
}
const glm::mat4& TransformComponent::GetLocalTransformMatrix() const
{
	if (bLocalDirty)
	{
		glm::mat4 TranslationMatrix = glm::translate(glm::mat4(1.0f), Position);
		glm::mat4 RotationMatrix = glm::mat4_cast(Rotation);
		glm::mat4 ScaleMatrix = glm::scale(glm::mat4(1.0f), Scale);

		CachedTransformMatrix = TranslationMatrix * RotationMatrix * ScaleMatrix;
		bLocalDirty = false;
	}

	return CachedTransformMatrix;
}

glm::mat4 TransformComponent::GetWorldTransformMatrix() const
{
	const glm::mat4& Local = GetLocalTransformMatrix();

	Entity* Owner = GetOwner();
	if (Owner && Owner->GetParent())
	{
		TransformComponent* ParentTransform = Owner->GetParent()->GetComponent<TransformComponent>();

		if (ParentTransform)
		{
			return ParentTransform->GetWorldTransformMatrix() * Local;
		}
	}

	return Local;
}
