module;

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <memory>

export module VulkanEngine;

import Renderer;
import Scene;
import ResourceManager;

// EngineSO Coordinate System Contract:
// - World space: right-handed, Y-up, camera looks toward -Z
// - GLM_FORCE_DEPTH_ZERO_TO_ONE: Vulkan depth range 0..1
// - GLM_FORCE_RIGHT_HANDED: explicit right-handed system
// - Projection Y flip ([1][1] *= -1): corrects GLM OpenGL assumption for Vulkan NDC
// - Front face: eCounterClockwise (natural right-handed, correct after Y flip)
// - Mouse X: negated (right-handed Y rotation convention)
// - Mouse Y: negated (GLFW Y-down + Y flip cancel each other out)

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

    void SetCursorCaptured(bool bCaptured);

	// Window dimensions for correct mouse input handling
	int WindowWidth = 1280;
	int WindowHeight = 720;

	// Input tracking
	double LastMouseX = 0.0;
	double LastMouseY = 0.0;
    bool bIgnoreNextMouseMove = false;

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