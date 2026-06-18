module;

#include <vulkan/vulkan_raii.hpp>

export module VulkanUtils;

export namespace VulkanUtils
{
	// Helper function to find suitable memory type index for a given type filter and properties
	uint32_t FindMemoryType(
		const vk::raii::PhysicalDevice& PhysDev,
		uint32_t TypeFilter,
		const vk::MemoryPropertyFlags& Properties);

    // Performs an image layout transition with correct pipeline barriers.
    // Throws std::invalid_argument for unsupported transition pairs.
	void TransitionImageLayout(
		vk::raii::CommandBuffer& CommandBuffer,
		vk::Image Image,
		vk::ImageAspectFlags Aspect,
		vk::ImageLayout OldLayout,
		vk::ImageLayout NewLayout);

	// Explicit — caller provides all sync parameters
	void TransitionImageLayout(
		vk::raii::CommandBuffer& CommandBuffer,
		vk::Image Image,
		vk::ImageAspectFlags Aspect,
		vk::ImageLayout OldLayout,
		vk::ImageLayout NewLayout,
		vk::PipelineStageFlags SrcStage,
		vk::PipelineStageFlags DstStage,
		vk::AccessFlags SrcAccess,
		vk::AccessFlags DstAccess);
}