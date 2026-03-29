module;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include<glm/gtc/quaternion.hpp>

module TransformComponent;

void TransformComponent::SetPosition(const glm::vec3& InPosition)
{
	Position = InPosition;
	bIsTransformDirty = true;
}

void TransformComponent::SetRotation(const glm::quat& InRotation)
{
	Rotation = InRotation;
	bIsTransformDirty = true;
}

void TransformComponent::SetScale(const glm::vec3& InScale)
{
	Scale = InScale;
	bIsTransformDirty = true;
}

const glm::mat4& TransformComponent::GetTransformMatrix() const
{
	if (bIsTransformDirty)
	{
		glm::mat4 TranslationMatrix = glm::translate(glm::mat4(1.0f), Position);
		glm::mat4 RotationMatrix = glm::mat4_cast(Rotation);
		glm::mat4 ScaleMatrix = glm::scale(glm::mat4(1.0f), Scale);

		CachedTransformMatrix = TranslationMatrix * RotationMatrix * ScaleMatrix;
		bIsTransformDirty = false;
	}

	return CachedTransformMatrix;
}
