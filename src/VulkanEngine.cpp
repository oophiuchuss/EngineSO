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
import EventSystem;
import EventBase;
import KeyEvent;
import MouseMovedEvent;
import MouseButtonEvent;

VulkanEngine::VulkanEngine()
    : Context()
{
    MainScene = std::make_unique<Scene>();
    ResourceManagerInstance = std::make_unique<ResourceManager>();

    // Context should be create with default constructor
    // Initialize all needed resources for vulkan and GLFW  
    InitWindow();
    InitVulkan();
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
	glfwSetKeyCallback(Window, KeyCallback);
	glfwSetCursorPosCallback(Window, MouseMoveCallback);
    glfwSetMouseButtonCallback(Window, MouseButtonCallback);
	glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Capture the mouse cursor for camera control
	LastFrameTime = glfwGetTime(); // Initialize last frame time for timing calculations
}

void VulkanEngine::InitVulkan()
{
    uint32_t GlfwExtensionCount = 0;
    const char** GlfwExtensions = glfwGetRequiredInstanceExtensions(&GlfwExtensionCount);

    // Collect GLFW required extensions
    std::vector<const char*> Extensions(GlfwExtensions, GlfwExtensions + GlfwExtensionCount);

    // Add debug utils extension for validation messenger
    Extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Configure intance
    vk::ApplicationInfo AppInfo("EngineSO", VK_MAKE_VERSION(1, 0, 0),
                                "EngineSO", VK_MAKE_VERSION(1, 0, 0),
                                VK_API_VERSION_1_4);

    // Enable validation layers if available
    std::vector<const char*> VulkanCreateLayers = {"VK_LAYER_KHRONOS_validation"};

    // Build messenger create info to chain into instance creation
    // This catches validation errors during vkCreateInstance and vkDestroyInstance
    // which happen before/after the messenger itself exists

    vk::DebugUtilsMessengerCreateInfoEXT MessengerCreateInfo(
        {},
        // Severity levels to report
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        // Message types to report
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        DebugCallback,
        nullptr);

    vk::InstanceCreateInfo CreateInfo(
        {},
        &AppInfo,
        static_cast<uint32_t>(VulkanCreateLayers.size()),   VulkanCreateLayers.data(),
        static_cast<uint32_t>(Extensions.size()),           Extensions.data());

	// Chain messenger create info into instance creation info
    CreateInfo.setPNext(&MessengerCreateInfo);

    Instance = vk::raii::Instance(Context, CreateInfo);

	// Create debug messenger after instance is created, but it will also catch messages during instance creation due to chaining
    DebugMessenger = Instance.createDebugUtilsMessengerEXT(MessengerCreateInfo);

    // Create Surface and move it to renderer
    VkSurfaceKHR RawSurface;

    if (glfwCreateWindowSurface(*Instance, Window, nullptr, &RawSurface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create surface for GLFW window");
    }

    vk::raii::SurfaceKHR Surface(Instance, std::move(RawSurface));
    RendererPtr = std::make_unique<Renderer>(Instance, std::move(Surface), ResourceManagerInstance.get());
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
		double CurrentTime = glfwGetTime();
		float DeltaTime = static_cast<float>(CurrentTime - LastFrameTime);
		LastFrameTime = CurrentTime;
        
        glfwPollEvents();

		MainScene->Update(DeltaTime);

        RendererPtr->RenderFrame(MainScene.get());
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanEngine::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT Severity, VkDebugUtilsMessageTypeFlagsEXT Type, const VkDebugUtilsMessengerCallbackDataEXT* CallbackData, void* UserData)
{
    // Filter by severity for clean output
    if (Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        std::cerr << "[Vulkan ERROR] " << CallbackData->pMessage << "\n\n";
    }
    else if (Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::cerr << "[Vulkan WARNING] " << CallbackData->pMessage << "\n\n";
    }
    else
    {
        std::cout << "[Vulkan VERBOSE] " << CallbackData->pMessage << "\n\n";
    }

    // Returning true would abort the Vulkan call that triggered this —
    // only the validation layers themselves should do that, not user code
    return VK_FALSE;
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
	WindowWidth = Width;
	WindowHeight = Height;

    // Forward to the renderer to recreate a swapchain
    if (RendererPtr)
    {
        RendererPtr->RecreateSwapchain();
    }
}

void VulkanEngine::KeyCallback(GLFWwindow* Window, int Key, int Scancode, int Action, int Mods)
{
    auto Engine = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(Window));

    if (Engine)
    {
        Engine->PublishKeyEvent(Key, Action);
    }
}

void VulkanEngine::PublishKeyEvent(int Key, int Action)
{
    KeyAction KAction = KeyAction::Press;

    switch (Action)
    {
	case GLFW_PRESS:    KAction = KeyAction::Press;     break;
	case GLFW_RELEASE:  KAction = KeyAction::Release;   break;
	case GLFW_REPEAT:   KAction = KeyAction::Repeat;    break;
    }

	EventSystem::Get().PublishEvent(KeyEvent(Key, KAction));
}

void VulkanEngine::MouseMoveCallback(GLFWwindow* Window, double XPos, double YPos)
{
    auto Engine = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(Window));

    if (Engine)
    {
        Engine->PublishMouseMoveEvent(XPos, YPos);
    }
}

void VulkanEngine::PublishMouseMoveEvent(double XPos, double YPos)
{
    if (bIgnoreNextMouseMove)
    {
        // Just sync position, publish nothing
        LastMouseX = XPos;
        LastMouseY = YPos;
        bIgnoreNextMouseMove = false;
        return;
    }

	double DeltaX = XPos - LastMouseX;
	double DeltaY = YPos - LastMouseY;
	LastMouseX = XPos;
	LastMouseY = YPos;

	EventSystem::Get().PublishEvent(MouseMovedEvent(DeltaX, DeltaY));
}

void VulkanEngine::MouseButtonCallback(GLFWwindow* Window, int Button, int Action, int Mods)
{
    auto* Engine = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(Window));
    if (Engine)
    {
        Engine->PublishMouseButtonEvent(Button, Action);
    }
}

void VulkanEngine::PublishMouseButtonEvent(int Button, int Action)
{
    // Capture / release cursor for right mouse button
    if (Button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (Action == GLFW_PRESS)
            SetCursorCaptured(true);
        else
            SetCursorCaptured(false);
    }

    MouseButtonAction BAction = (Action == GLFW_PRESS) ? MouseButtonAction::Press
                                                       : MouseButtonAction::Release;

    EventSystem::Get().PublishEvent(MouseButtonEvent(Button, BAction));
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

void VulkanEngine::SetCursorCaptured(bool bCaptured)
{
    if (bCaptured)
    {
        glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		bIgnoreNextMouseMove = true; // Ignore the next mouse move event to prevent sudden camera jump
    }
    else
    {
        glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}
