module;

#include <glm/glm.hpp>

module FlyCameraControllerComponent;

FlyCameraControllerComponent::FlyCameraControllerComponent() : MovementSpeed(5.0f), MouseSensitivity(0.1f)
{
	UpdateCameraVectors();
}

void FlyCameraControllerComponent::ProcessKeyboardInput(/*const CameraMovement& Direction,*/ float DeltaTime)
{
	/*float Velocity = MovementSpeed * DeltaTime;

	switch (Direction)
	{
	case CameraMovement::FORWARD:
		Position += Front * Velocity;
		break;
	case CameraMovement::BACKWARD:
		Position -= Front * Velocity;
		break;
	case CameraMovement::LEFT:
		Position -= Right * Velocity;
		break;
	case CameraMovement::RIGHT:
		Position += Right * Velocity;
		break;
	case CameraMovement::UP:
		Position += Up * Velocity;
		break;
	case CameraMovement::DOWN:
		Position -= Up * Velocity;
		break;
	default:
		break;
	}*/
}

void FlyCameraControllerComponent::ProcessMouseMovement(float XOffset, float YOffset, bool bConstrainPitch)
{
	/*XOffset *= MouseSensitivity;
	YOffset *= MouseSensitivity;

	Yaw += XOffset;
	Pitch += YOffset;

	if (bConstrainPitch)
	{
		Pitch = std::clamp(Pitch, -89.0f, 89.0f);
	}

	UpdateCameraVectors();*/
}

void FlyCameraControllerComponent::ProcessMouseScroll(float YOffset)
{
	// TODO: implement zooming in and out by adjusting the field of view (Zoom variable)
}

void FlyCameraControllerComponent::UpdateCameraVectors()
{
	// Calculate the new Front vector
/*	glm::vec3 NewFront;
	NewFront.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
	NewFront.y = sin(glm::radians(Pitch));
	NewFront.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
	Front = glm::normalize(NewFront);

	// Recalculate Right and Up vectors
	Right = glm::normalize(glm::cross(Front, WorldUp));
	Up = glm::normalize(glm::cross(Right, Front));*/
}
