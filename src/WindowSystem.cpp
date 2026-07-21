module;

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>
#include <stdexcept>
#include <string>
#include <memory>

module WindowSystem;

import EventSystem;
import EventBase;
import EventDispatcher;
import CursorCaptureRequestEvent;
import KeyEvent;
import MouseMovedEvent;
import MouseButtonEvent;
import MouseScrollEvent;
import WindowResizeEvent;

WindowSystem::WindowSystem(
	EventSystem* InEventSystem,
	int InWidth,
	int InHeight, 
	const std::string& InTitle)
	: EventSystemPtr(InEventSystem),
	WindowWidth(InWidth),
	WindowHeight(InHeight)
{
    EventSystemPtr->AddListener(this, static_cast<int>(EventCategory::Window), 1);

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    Window = glfwCreateWindow(WindowWidth, WindowHeight, InTitle.c_str(), nullptr, nullptr);
    if (!Window)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    // Store a pointer to this engine instance so the callback can access to it
    glfwSetWindowUserPointer(Window, this);
	RegisterCallbacks();
    SetCursorCaptured(false);
}

WindowSystem::~WindowSystem()
{
    if (EventSystemPtr)
    {
        EventSystemPtr->RemoveListener(this);
    }

    // Destroy Window and GLFW
    if (Window)
    {
        glfwDestroyWindow(Window);
        Window = nullptr;
    }

    glfwTerminate();
}

void WindowSystem::PollEvents()
{
    glfwPollEvents();
}

bool WindowSystem::ShouldClose() const
{
    return glfwWindowShouldClose(Window);
}

void WindowSystem::SetCursorCaptured(bool bCaptured)
{
    bCursorCaptured = bCaptured;
    if (bCursorCaptured)
    {
        glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        bIgnoreNextMouseMove = true; // Ignore the next mouse move event to prevent sudden camera jump
    }
    else
    {
        glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

vk::raii::SurfaceKHR WindowSystem::CreateVulkanSurface(const vk::raii::Instance& Instance) const
{
    // Create Surface and move it to renderer
    VkSurfaceKHR RawSurface;

    if (glfwCreateWindowSurface(*Instance, Window, nullptr, &RawSurface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create surface for GLFW window");
    }

    return vk::raii::SurfaceKHR(Instance, std::move(RawSurface));
}

void WindowSystem::RegisterCallbacks()
{
    glfwSetFramebufferSizeCallback(Window, FrameBufferResizeCallback);
    glfwSetKeyCallback(Window, KeyCallback);
    glfwSetCursorPosCallback(Window, MouseMoveCallback);
    glfwSetMouseButtonCallback(Window, MouseButtonCallback);
    glfwSetScrollCallback(Window, MouseScrollCallback);
}

EventReply WindowSystem::OnEvent(const EventBase& Event)
{
    EventDispatcher Dispatcher(Event);

    if (Dispatcher.Dispatch<CursorCaptureRequestEvent>(
        [this](const CursorCaptureRequestEvent& E)
        {
            SetCursorCaptured(E.ShouldCapture());
        }))
    {
        return EventReply::Handled;
    }

    return EventReply::Unhandled;
}

void WindowSystem::FrameBufferResizeCallback(GLFWwindow* Window, int Width, int Height)
{
    auto WindowSystemPtr = reinterpret_cast<WindowSystem*>(glfwGetWindowUserPointer(Window));

    if (WindowSystemPtr)
    {
        WindowSystemPtr->OnResize(Width, Height);
    }
}

void WindowSystem::KeyCallback(GLFWwindow* Window, int Key, int Scancode, int Action, int Mods)
{
    auto WindowSystemPtr = reinterpret_cast<WindowSystem*>(glfwGetWindowUserPointer(Window));

    if (WindowSystemPtr)
    {
        WindowSystemPtr->OnKey(Key, Action);
    }
}

void WindowSystem::MouseMoveCallback(GLFWwindow* Window, double XPos, double YPos)
{
    auto WindowSystemPtr = reinterpret_cast<WindowSystem*>(glfwGetWindowUserPointer(Window));

    if (WindowSystemPtr)
    {
        WindowSystemPtr->OnMouseMove(XPos, YPos);
    }
}

void WindowSystem::MouseButtonCallback(GLFWwindow* Window, int Button, int Action, int Mods)
{
    auto WindowSystemPtr = reinterpret_cast<WindowSystem*>(glfwGetWindowUserPointer(Window));

    if (WindowSystemPtr)
    {
        WindowSystemPtr->OnMouseButton(Button, Action);
    }
}

void WindowSystem::MouseScrollCallback(GLFWwindow* W, double XOffset, double YOffset)
{
	auto WindowSystemPtr = reinterpret_cast<WindowSystem*>(glfwGetWindowUserPointer(W));
	if (WindowSystemPtr)
	{
		WindowSystemPtr->OnMouseScroll(XOffset, YOffset);
	}
}

void WindowSystem::OnResize(int Width, int Height)
{
	WindowWidth = Width;
	WindowHeight = Height;

    EventSystemPtr->PublishEvent(WindowResizeEvent(WindowWidth, WindowHeight));
}

void WindowSystem::OnKey(int Key, int Action)
{
    KeyAction KAction;
    switch (Action)
    {
    case GLFW_PRESS:   KAction = KeyAction::Press;   break;
    case GLFW_RELEASE: KAction = KeyAction::Release; break;
    case GLFW_REPEAT:  KAction = KeyAction::Repeat;  break;
    default:           return;
    }
    EventSystemPtr->PublishEvent(KeyEvent(Key, KAction));
}

void WindowSystem::OnMouseMove(double XPos, double YPos)
{
    if (bIgnoreNextMouseMove)
    {
        LastMouseX = XPos;
        LastMouseY = YPos;
        bIgnoreNextMouseMove = false;
        return;
    }

    double DeltaX = XPos - LastMouseX;
    double DeltaY = YPos - LastMouseY;
    LastMouseX = XPos;
    LastMouseY = YPos;

    EventSystemPtr->PublishEvent(MouseMovedEvent(DeltaX, DeltaY));
}

void WindowSystem::OnMouseButton(int Button, int Action)
{
    MouseButtonAction BAction = (Action == GLFW_PRESS)
        ? MouseButtonAction::Press
        : MouseButtonAction::Release;

    EventSystemPtr->PublishEvent(MouseButtonEvent(Button, BAction));
}

void WindowSystem::OnMouseScroll(double XOffset, double YOffset)
{
    EventSystemPtr->PublishEvent(MouseScrollEvent(XOffset, YOffset));
}
