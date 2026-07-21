module;

#include <glm/glm.hpp>
#include <unordered_set>

export module FlyCameraControllerComponent;

import Component;
import EventSystem;
import EventListener;
import EventBase;
import MouseMovedEvent;
import MouseButtonEvent;
import MouseScrollEvent;
import KeyEvent;

export class FlyCameraControllerComponent : public ComponentBase, public EventListener
{
public:
	explicit FlyCameraControllerComponent(EventSystem* InEventSystem)
		: EventSystemPtr(InEventSystem) {
	}

protected:
	// Component lifecycle
	void OnInitialize() override;
	void OnDestroy() override;
	void Update(float DeltaTime) override;

	// Event handling
	EventReply OnEvent(const EventBase& Event) override;

	// Configuration
	void SetMovementSpeed(float speed) { MovementSpeed = speed; }
	void SetMouseSensitivity(float sensitivity) { MouseSensitivity = sensitivity; }

private:
	void ProcessKeyEvent(const KeyEvent& Event);
	void ProcessMouseMovement(const MouseMovedEvent& Event);
	void ProcessMouseButtonEvent(const MouseButtonEvent& Event);
	void ProcessMouseScroll(const MouseScrollEvent& Event);

	EventSystem* EventSystemPtr = nullptr;

	std::unordered_set<int> HeldKeys; // Set of currently held keys (using key codes)

	// Mouse movement tracking
	double MouseDeltaX = 0.0;
	double MouseDeltaY = 0.0;

	float Yaw = -90.0f;	// Initialize to look along the negative Z-axis
	float Pitch = 0.0f;

	// Users interaction parameters
	float MovementSpeed = 5.0f;		// Speed of camera movement
	float MouseSensitivity = 0.1f;	// Sensitivity of mouse movement for camera rotation

	// Limits for movement speed and sensitivity to prevent extreme values
	float MinMovementSpeed = 0.5f;
	float MaxMovementSpeed = 50.0f;
	float ScrollSensitivity = 1.0f;
};

