module;

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <memory>

export module VulkanEngine;

import Renderer;
import Scene;
import ResourceManager;

export class VulkanEngine
{
public:
    VulkanEngine();
    ~VulkanEngine();

    void Run();

	Renderer* GetRenderer() const { return RendererPtr.get(); }
	Scene* GetMainScene() const { return MainScene.get(); }
	ResourceManager* GetResourceManager() const { return ResourceManagerInstance.get(); }

private:
    void InitWindow();
    void InitVulkan();
    void MainLoop();
    void Cleanup();

	// Input tracking
	double LastMouseX = 0.0;
	double LastMouseY = 0.0;
    bool bFirstCursorEvent = true;

	// Timing
    double LastFrameTime = 0.0;

    // Window Handle
    GLFWwindow* Window = nullptr;

    // Vulkan base objects (should live the entire engine lifetime)
    vk::raii::Context Context;  
    vk::raii::Instance Instance = nullptr;

    std::unique_ptr<Renderer> RendererPtr;

	std::unique_ptr<Scene> MainScene;

	std::unique_ptr<ResourceManager> ResourceManagerInstance;

    static void FrameBufferResizeCallback(GLFWwindow* Window, int Width, int Height);
    void OnResize(int Width, int Height);

    static void KeyCallback(GLFWwindow* Window, int Key, int Scancode, int Action, int Mods);
    void PublishKeyEvent(int Key, int Action);

    static void MouseMoveCallback(GLFWwindow* Window, double XPos, double YPos);
    void PublishMouseMoveEvent(double XPos, double YPos);

    static void MouseButtonCallback(GLFWwindow* Window, int Button, int Action, int Mods);
    void PublishMouseButtonEvent(int Button, int Action);
};