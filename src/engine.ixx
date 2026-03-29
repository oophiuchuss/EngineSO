module;

#include <vulkan/vulkan_raii.hpp>

export module Engine;

export class VulkanEngine
{
public:
    VulkanEngine();
    void run();
private:
    vk::raii::Context context;  
    vk::raii::Instance instance;
};