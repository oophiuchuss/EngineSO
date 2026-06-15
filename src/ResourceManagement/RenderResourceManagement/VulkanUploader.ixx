module;

#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <functional>

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
	struct UploadBufferResult
	{
		vk::raii::Buffer Buffer;
		vk::raii::DeviceMemory Memory;
	};

	// Upload raw pixels to a device‑local image (shader‑ready)
	struct UploadImageResult
	{
		vk::raii::Image        Image;
		vk::raii::DeviceMemory Memory;
	};

	UploadBufferResult UploadBuffer(
		const void* Data,
		vk::DeviceSize Size,
		vk::BufferUsageFlags TargetUsage);

	UploadImageResult UploadImage(
		const void* PixelData, 
		uint32_t Width, 
		uint32_t Height, 
		vk::Format Format);

private:
	struct StagingBuffer
	{
		vk::raii::Buffer Buffer;
		vk::raii::DeviceMemory Memory;
	};

	StagingBuffer CreateStagingBuffer(const void* Data, vk::DeviceSize Size);

	UploadBufferResult CreateDeviceLocalBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage);

	// Image helpers
	UploadImageResult CreateDeviceLocalImage(uint32_t Width, uint32_t Height, vk::Format Format);

	// Submit a one‑time command buffer and wait for completion
	void SubmitCopy(std::function<void(vk::raii::CommandBuffer&)> RecordCommands);
	
	const vk::raii::Device& Device;
	const vk::raii::PhysicalDevice& PhysicalDevice;
	const vk::raii::Queue& TransferQueue;
	uint32_t TransferQueueFamilyIndex;
	uint32_t GraphicsQueueFamilyIndex;
	bool bSameQueueFamily;

	vk::raii::CommandPool TransferCommandPool = nullptr;
	vk::raii::Fence UploadFence = nullptr;
};