module;

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <memory>

module VulkanEngine;

import Renderer;

import Component;
import Entity;
import MeshComponent;
import TransformComponent;



VulkanEngine::VulkanEngine()
    : Context()
{
    // Context should be create with default constructor
    // Initialize all needed resources for vulkan and GLFW  
    InitWindow();
    InitVulkan();

	MainScene = std::make_unique<Scene>();
}

VulkanEngine::~VulkanEngine()
{
    Cleanup();
}

void VulkanEngine::InitWindow()
{
    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); 

    Window = glfwCreateWindow(1280, 720, "EngineSO", nullptr, nullptr);
    if (!Window)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    // Store a pointer to this engine instance so the callback can access to it
    glfwSetWindowUserPointer(Window, this);
    glfwSetFramebufferSizeCallback(Window, FrameBufferResizeCallback);
}

void VulkanEngine::InitVulkan()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // Configure intance
    vk::ApplicationInfo AppInfo("EngineSO", VK_MAKE_VERSION(1, 0, 0),
                                "EngineSO", VK_MAKE_VERSION(1, 0, 0),
                                VK_API_VERSION_1_4);

    // Enable validation layers if available
    std::vector<const char*> VulkanCreateLayers = {"VK_LAYER_KHRONOS_validation"};

    vk::InstanceCreateInfo CreateInfo({}, &AppInfo,
        (uint32_t)VulkanCreateLayers.size(), VulkanCreateLayers.data(),
        glfwExtensionCount, glfwExtensions);

    Instance = vk::raii::Instance(Context, CreateInfo);

    // Create Surface and move it to renderer
    VkSurfaceKHR RawSurface;

    if (glfwCreateWindowSurface(*Instance, Window, nullptr, &RawSurface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create surface for GLFW window");
    }

    vk::raii::SurfaceKHR Surface(Instance, std::move(RawSurface));

    RendererPtr = std::make_unique<Renderer>(Instance, std::move(Surface));
}

void VulkanEngine::Run()
{
    std::cout << "Vulkan instance and renderer created successfully!" << std::endl;
    MainLoop();
}

void VulkanEngine::MainLoop()
{
    while (!glfwWindowShouldClose(Window))
    {
        glfwPollEvents();

        RendererPtr->RenderFrame(MainScene.get());
    }
}

void VulkanEngine::FrameBufferResizeCallback(GLFWwindow* Window, int Width, int Height)
{
    auto Engine = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(Window));

    if (Engine)
    {
        Engine->OnResize(Width, Height);
    }
}

void VulkanEngine::OnResize(int Width, int Height)
{
    // Forward to the renderer to recreate a swapchain
    if (RendererPtr)
    {
        RendererPtr->RecreateSwapchain();
    }
}

void VulkanEngine::Cleanup()
{
    MainScene.reset();

    // Destroy renderer first as it should release GPU resources before vulkan instance is destroyed
    RendererPtr.reset();

    // Destroy Window and GLFW
    if (Window)
    {
        glfwDestroyWindow(Window);
        Window = nullptr;
    }

    glfwTerminate();
}
