module;

#include <vulkan/vulkan_raii.hpp>
#include <iostream>
#include <stdexcept>
#include <memory>

module VulkanEngine;

import Paths;

import EventSystem;
import WindowSystem;
import WindowResizeEvent;
import Renderer;
import Scene;
import ResourceManager;
import TaskScheduler;

VulkanEngine::VulkanEngine()
{
    // Set paths first
    Paths::SetAssetsRoot(std::string(SOURCE_DIR) + "/assets/");
    Paths::SetShadersRoot(std::string(BUILD_DIR) + "/shaders/");
    Paths::SetCacheRoot(std::string(BUILD_DIR) + "/cache/");

	EventSystemInstance = std::make_unique<EventSystem>();

    WindowSystemInstance = std::make_unique<WindowSystem>(EventSystemInstance.get(), 1280, 720, "EngineSO");

    ResourceManagerInstance = std::make_unique<ResourceManager>();

    TaskSchedulerInstance = std::make_unique<TaskScheduler>();

    // Context should be create with default constructor
    // Initialize all needed resources for vulkan and GLFW  
    InitVulkan();

    RendererInstance = std::make_unique<Renderer>(
        Instance,
        WindowSystemInstance->CreateVulkanSurface(Instance),
        ResourceManagerInstance.get());
    
    MainSceneInstance = std::make_unique<Scene>(EventSystemInstance.get());

    EventSystemInstance->AddListener(RendererInstance.get(),
        static_cast<int>(EventCategory::Window) | 
        static_cast<int>(EventCategory::Scene));

    LastFrameTime = 0.0;
}

void VulkanEngine::InitVulkan()
{
    uint32_t WindowExtensionCount = 0;
    const char** WindowExtensions = WindowSystemInstance->GetRequiredInstanceExtensions(&WindowExtensionCount);

    // Collect Window required extensions
    std::vector<const char*> Extensions(WindowExtensions, WindowExtensions + WindowExtensionCount);

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
}

void VulkanEngine::Run()
{
    MainLoop();
}

void VulkanEngine::MainLoop()
{
    while (!WindowSystemInstance->ShouldClose())
    {
        double CurrentTime = WindowSystemInstance->GetTime();
        float  DeltaTime = static_cast<float>(CurrentTime - LastFrameTime);
        LastFrameTime = CurrentTime;

        WindowSystemInstance->PollEvents();     // fires window callbacks → EventSystem
        EventSystemInstance->ProcessQueue();    // flush deferred events if any
        MainSceneInstance->Update(DeltaTime);
        RendererInstance->RenderFrame(MainSceneInstance.get());
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