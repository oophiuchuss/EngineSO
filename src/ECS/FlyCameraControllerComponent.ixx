module;

#include <glm/glm.hpp>

export module FlyCameraControllerComponent;

import Component;

export class FlyCameraControllerComponent : public ComponentBase
{
public:
	FlyCameraControllerComponent();

	// Input processing functions
	// TODO: implement input handling
	void ProcessKeyboardInput(/*const CameraMovement& Direction,*/ float DeltaTime); 
	void ProcessMouseMovement(float XOffset, float YOffset, bool bConstrainPitch = true);
	void ProcessMouseScroll(float YOffset);

	// Configuration
	void SetMovementSpeed(float speed) { MovementSpeed = speed; }
	void SetMouseSensitivity(float sensitivity) { MouseSensitivity = sensitivity; }

private:
	// Internal coordinate system maintenance
	void UpdateCameraVectors();

	// Users interaction parameters
	float MovementSpeed;	// Speed of camera movement
	float MouseSensitivity;	// Sensitivity of mouse movement for camera rotation
};

