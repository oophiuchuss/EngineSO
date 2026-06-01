module;

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <GLFW/glfw3.h>

module FlyCameraControllerComponent;

import Entity;
import EventSystem;
import EventDispatcher;
import KeyEvent;
import TransformComponent;
import MouseButtonEvent;

void FlyCameraControllerComponent::OnInitialize()
{
	EventSystem::Get().AddListener(this,
		static_cast<int>(EventCategory::Keyboard) |
		static_cast<int>(EventCategory::Mouse) |
		static_cast<int>(EventCategory::MouseButton),
		0);
}

void FlyCameraControllerComponent::OnDestroy()
{
	EventSystem::Get().RemoveListener(this);
}

void FlyCameraControllerComponent::Update(float DeltaTime)
{
	auto* Transform = GetOwner()->GetComponent<TransformComponent>();
	if (!Transform)
	{
		return;
	}

	constexpr int RightMouseButton = GLFW_MOUSE_BUTTON_RIGHT;   // 1
	bool bMouseHold = (HeldKeys.find(RightMouseButton) != HeldKeys.end());

	if (bMouseHold)
	{
		glm::vec3 Velocity(0.0f);
		glm::quat Rotation = Transform->GetRotation();
		glm::vec3 Forward = Rotation * glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 Right = Rotation * glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 Up = Rotation * glm::vec3(0.0f, 1.0f, 0.0f);

		// Map GLFW key codes to movement directions
		for (int Key : HeldKeys)
		{
			switch (Key)
			{
			case GLFW_KEY_W: Velocity += Forward; break;
			case GLFW_KEY_S: Velocity -= Forward; break;
			case GLFW_KEY_A: Velocity -= Right; break;
			case GLFW_KEY_D: Velocity += Right; break;
			case GLFW_KEY_Q: Velocity -= Up; break;
			case GLFW_KEY_E: Velocity += Up; break;
			}
		}

		if (glm::length(Velocity) > 0.0f)
		{
			Velocity = glm::normalize(Velocity) * MovementSpeed * DeltaTime;
		}

		Transform->SetPosition(Transform->GetPosition() + Velocity);
	}

	if (bMouseHold)
	{
		// Mouse movement for rotation
		Yaw += static_cast<float>(MouseDeltaX) * MouseSensitivity;
		Pitch += static_cast<float>(MouseDeltaY) * MouseSensitivity;
		Pitch = glm::clamp(Pitch, -89.0f, 89.0f); // Prevent gimbal lock

		glm::quat YawQuat = glm::angleAxis(glm::radians(Yaw), glm::vec3(0.0f, -1.0f, 0.0f));
		glm::quat PitchQuat = glm::angleAxis(glm::radians(Pitch), glm::vec3(1.0f, 0.0f, 0.0f));
		Transform->SetRotation(YawQuat * PitchQuat);
	}

	// Reset mouse deltas after applying rotation
	MouseDeltaX = 0.0;
	MouseDeltaY = 0.0;
}

void FlyCameraControllerComponent::OnEvent(const EventBase& Event)
{
	EventDispatcher Dispatcher(Event);

	Dispatcher.Dispatch<KeyEvent>([this](const KeyEvent& e)
	{
		ProcessKeyEvent(e);
	});

	Dispatcher.Dispatch<MouseMovedEvent>([this](const MouseMovedEvent& e)
	{
		ProcessMouseMovement(e);
	});

	Dispatcher.Dispatch<MouseButtonEvent>([this](const MouseButtonEvent& e) 
	{
		ProcessMouseButtonEvent(e);
	});
}

void FlyCameraControllerComponent::ProcessKeyEvent(const KeyEvent& Event)
{
	int Key = Event.GetKeyCode();
	auto Action = Event.GetAction();

	if (Action == KeyAction::Press || Action == KeyAction::Repeat)
	{
		HeldKeys.insert(Key);
	}
	else if (Action == KeyAction::Release)
	{
		HeldKeys.erase(Key);
	}
}

void FlyCameraControllerComponent::ProcessMouseMovement(const MouseMovedEvent& Event)
{
	MouseDeltaX += Event.GetXOffset();
	MouseDeltaY += Event.GetYOffset();
}

void FlyCameraControllerComponent::ProcessMouseButtonEvent(const MouseButtonEvent& Event)
{
	int Button = Event.GetButton();
	if (Event.GetAction() == MouseButtonAction::Press)
	{
		HeldKeys.insert(Button);
	}
	else
	{
		HeldKeys.erase(Button);
	}
}

