module;

#include <vulkan/vulkan_raii.hpp>
#include <iostream>

module Engine;

VulkanEngine::VulkanEngine()
    : context(),
      instance(context, [](){
          vk::ApplicationInfo appInfo("Hello Vulkan", 1, "EngineSO", 1, VK_API_VERSION_1_4);
          return vk::InstanceCreateInfo({}, &appInfo);
      }())
{
}

void VulkanEngine::run() 
{
        std::cout << "Vulkan instance created successfully!" << std::endl;
}
