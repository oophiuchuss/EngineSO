module;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include<glm/gtc/quaternion.hpp>

module TransformComponent;

import Entity;

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
