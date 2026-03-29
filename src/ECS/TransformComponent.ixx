module;

#include <glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

export module TransformComponent;

import Component;

export class TransformComponent : public ComponentBase
{

public:
	void SetPosition(const glm::vec3& InPosition);
	void SetRotation(const glm::quat& InRotation);
	void SetScale(const glm::vec3& InScale);
	
	const glm::vec3& GetPosition() const { return Position; }
	const glm::quat& GetRotation() const { return Rotation; }
	const glm::vec3& GetScale() const { return Scale; }

	const glm::mat4& GetTransformMatrix() const;

private:
	// TODO: consider using Transform struct to store these values instead of separate variables
	glm::vec3 Position = glm::vec3(0.0f);
	glm::quat Rotation = glm::vec3(0.0f);
	glm::vec3 Scale = glm::vec3(0.0f);

	mutable glm::mat4 CachedTransformMatrix = glm::mat4(1.0f);
	mutable bool bIsTransformDirty = true;

};
