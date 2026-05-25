module;

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <stdexcept>

export module VulkanUtils;

export namespace VulkanUtils
{
	// Helper function to find suitable memory type index for a given type filter and properties
	uint32_t FindMemoryType(const vk::raii::PhysicalDevice& PhysDev, uint32_t TypeFilter, const vk::MemoryPropertyFlags& Properties)
	{
        vk::PhysicalDeviceMemoryProperties MemProperties = PhysDev.getMemoryProperties();

        for (uint32_t i = 0; i < MemProperties.memoryTypeCount; i++)
        {
            if (TypeFilter & (1 << i))
            {
                vk::MemoryPropertyFlags RequiredPropertiesMatch = MemProperties.memoryTypes[i].propertyFlags & Properties;

                if (RequiredPropertiesMatch == Properties)
                {
                    return i;
                }
            }

        }

        throw std::runtime_error("Failed to find suitable memory type");
	}
}