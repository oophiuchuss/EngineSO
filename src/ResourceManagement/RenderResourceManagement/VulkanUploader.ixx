module;

#include <vulkan/vulkan_raii.hpp>
#include <vector>

export module VulkanUploader;

export class VulkanUploader
{
public:
	VulkanUploader(
		const vk::raii::Device& Device,
		const vk::raii::PhysicalDevice& PhysicalDevice,
		uint32_t TransferQueueFamilyIndex,
		uint32_t GraphicsQueueFamilyIndex,
		const vk::raii::Queue& TransferQueue);

	~VulkanUploader() = default;


	// Upload raw bytes to a device-local GPU buffer
	// Returns the final device-local buffer ready for rendering
	// Handles staging, copy, and queue family ownership transfer internally
	struct UploadResult
	{
		vk::raii::Buffer Buffer;
		vk::raii::DeviceMemory Memory;
	};

	UploadResult Upload(
		const void* Data,
		vk::DeviceSize Size,
		vk::BufferUsageFlags TargetUsage);

private:
	struct StagingBuffer
	{
		vk::raii::Buffer Buffer;
		vk::raii::DeviceMemory Memory;
	};

	StagingBuffer CreateStagingBuffer(const void* Data, vk::DeviceSize Size);

	UploadResult CreateDeviceLocalBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage);

	void SubmitCopy(vk::Buffer Src, vk::Buffer Dst, vk::DeviceSize Size);

	const vk::raii::Device& Device;
	const vk::raii::PhysicalDevice& PhysicalDevice;
	const vk::raii::Queue& TransferQueue;
	uint32_t TransferQueueFamilyIndex;
	uint32_t GraphicsQueueFamilyIndex;
	bool bSameQueueFamily;

	vk::raii::CommandPool TransferCommandPool = nullptr;
	vk::raii::Fence UploadFence = nullptr;
};