module;

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <memory>

export module VulkanEngine;

import Renderer;
import Scene;
import ResourceManager;
import TaskScheduler;
import EventSystem;
import WindowSystem;
import ImGuiSystem;

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
    ~VulkanEngine() = default;

    void Run();

	Renderer* GetRenderer() const { return RendererInstance.get(); }
	Scene* GetMainScene() const { return MainSceneInstance.get(); }
	ResourceManager* GetResourceManager() const { return ResourceManagerInstance.get(); }
    TaskScheduler* GetTaskScheduler() const { return TaskSchedulerInstance.get(); }
    EventSystem* GetEventSystem() const { return EventSystemInstance.get(); }
	WindowSystem* GetWindowSystem() const { return WindowSystemInstance.get(); }

private:
    void InitVulkan();
    void MainLoop();

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      Severity,
        VkDebugUtilsMessageTypeFlagsEXT             Type,
        const VkDebugUtilsMessengerCallbackDataEXT* CallbackData,
        void* UserData);

    std::unique_ptr<EventSystem> EventSystemInstance;
    std::unique_ptr<WindowSystem> WindowSystemInstance;
	std::unique_ptr<ResourceManager> ResourceManagerInstance;
	std::unique_ptr<TaskScheduler> TaskSchedulerInstance;

    // Vulkan base objects (should live the entire engine lifetime)
    vk::raii::Context Context;  
    vk::raii::Instance Instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT DebugMessenger = nullptr;

    std::unique_ptr<ImGuiSystem> ImGuiSystemInstance;
    std::unique_ptr<Renderer> RendererInstance;
	std::unique_ptr<Scene> MainSceneInstance;

    double LastFrameTime = 0.0;

};