module;

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <memory>

export module VulkanEngine;

import Renderer;


import Entity;

export class VulkanEngine
{
public:
    VulkanEngine();
    ~VulkanEngine();

    void Run();

	Renderer* GetRenderer() const { return RendererPtr.get(); }

private:
    void InitWindow();
    void InitVulkan();
    void MainLoop();
    void Cleanup();

    // Window Handle
    GLFWwindow* Window = nullptr;

    // Vulkan base objects (should live the entire engine lifetime)
    vk::raii::Context Context;  
    vk::raii::Instance Instance = nullptr;

    std::unique_ptr<Renderer> RendererPtr;

    static void FrameBufferResizeCallback(GLFWwindow* Window, int Width, int Height);
    void OnResize(int Width, int Height);

    std::unique_ptr<Entity> RenderEntity;
};