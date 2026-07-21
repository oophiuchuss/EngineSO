module;

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>
#include <string>
#include <memory>

export module WindowSystem;

import EventSystem;
import EventListener;
import EventBase;

export class WindowSystem : public EventListener
{
public:
	WindowSystem(EventSystem* InEventSystem,
		int InWidth, int InHeight,
		const std::string& InTitle);

	~WindowSystem();

	// Non-copyable, non-movable — owns GLFW window handle
	WindowSystem(const WindowSystem&) = delete;
	WindowSystem& operator=(const WindowSystem&) = delete;

	void PollEvents();
	bool ShouldClose() const;

	// Cursor management
	void SetCursorCaptured(bool bCaptured);
	inline bool IsCursorCaptured() const { return bCursorCaptured; }

	// Surface creation
	vk::raii::SurfaceKHR CreateVulkanSurface(const vk::raii::Instance& Instance) const;

	inline const char** GetRequiredInstanceExtensions(uint32_t* WindowExtensionCount) { return glfwGetRequiredInstanceExtensions(WindowExtensionCount); }

	inline float GetTime() const { return static_cast<float>(glfwGetTime()); }

	GLFWwindow* GetHandle() const { return Window; }
	int GetWidth() const { return WindowWidth; }
	int GetHeight() const { return WindowHeight; }
private:
	void RegisterCallbacks();

	EventReply OnEvent(const EventBase& Event) override;

	// Static GLFW callback functions that forward to the instance methods
	static void FrameBufferResizeCallback(GLFWwindow* Window, int Width, int Height);
	static void KeyCallback(GLFWwindow* Window, int Key, int Scancode, int Action, int Mods);
	static void MouseMoveCallback(GLFWwindow* Window, double XPos, double YPos);
	static void MouseButtonCallback(GLFWwindow* Window, int Button, int Action, int Mods);
	static void MouseScrollCallback(GLFWwindow* W, double XOffset, double YOffset);


	// Instance methods to handle the events and publish them to the event system
	void OnResize(int Width, int Height);
	void OnKey(int Key, int Action);
	void OnMouseMove(double XPos, double YPos);
	void OnMouseButton(int Button, int Action);
	void OnMouseScroll(double XOffset, double YOffset);

	EventSystem* EventSystemPtr = nullptr;

	GLFWwindow*	Window = nullptr;
	int	WindowWidth = 0;
	int WindowHeight = 0;

	double LastMouseX = 0.0;
	double LastMouseY = 0.0;
	bool bIgnoreNextMouseMove = false;
	bool bCursorCaptured = false;

};