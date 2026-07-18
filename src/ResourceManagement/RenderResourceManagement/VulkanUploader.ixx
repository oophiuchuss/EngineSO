module;

#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <functional>
#include <span>
#include <memory>

export module VulkanUploader;

import Shader;
import PipelineCache;

export class VulkanUploader
{
public:
	VulkanUploader(
		const vk::raii::Device& Device,
		const vk::raii::PhysicalDevice& PhysicalDevice,
		uint32_t TransferQueueFamilyIndex,
		uint32_t GraphicsQueueFamilyIndex,
		const vk::raii::Queue& TransferQueue,
		const vk::raii::Queue& GraphicsQueue);

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
		uint32_t MipLevels;
	};

	// Descriptor for a single buffer in a batch upload
	struct BufferUploadInfo
	{
		const void* Data = nullptr;
		vk::DeviceSize Size = 0;
		vk::BufferUsageFlags TargetUsage = {};
	};

	enum class ImageMipGeneration
	{
		LinearBlit,
		NormalMapCompute
	};

	struct ImageUploadInfo
	{
		const void* PixelData = nullptr;
		uint32_t    Width = 0;
		uint32_t    Height = 0;
		vk::Format  Format = vk::Format::eR8G8B8A8Unorm;
		ImageMipGeneration MipGeneration = ImageMipGeneration::LinearBlit;
	};

	UploadBufferResult UploadBuffer(
		const void* Data,
		vk::DeviceSize Size,
		vk::BufferUsageFlags TargetUsage);

	UploadImageResult UploadImage(const ImageUploadInfo& Image);

	// Upload many buffers in a single command buffer — one submit, one fence wait.
	std::vector<UploadBufferResult> UploadBufferBatch(std::span<const BufferUploadInfo> Infos);

	// Upload many images in a single command buffer — one submit, one fence wait.
	std::vector<UploadImageResult> UploadImageBatch(std::span<const ImageUploadInfo> Images);

	// Initialize the compute pipeline for normal map mip generation
	void InitializeNormalMipGeneration(Shader& InShader, PipelineCache& InPipelineCache);

private:
	struct StagingBuffer
	{
		vk::raii::Buffer Buffer;
		vk::raii::DeviceMemory Memory;
	};

	struct NormalMipResources
	{
		// Declaration order matters: destruction happens in reverse.
		// Sets are therefore destroyed before their pool.
		vk::raii::DescriptorPool DescriptorPool = nullptr;
		std::vector<vk::raii::ImageView> MipViews;
		std::vector<vk::raii::DescriptorSet> DescriptorSets;
	};

	StagingBuffer CreateStagingBuffer(const void* Data, vk::DeviceSize Size);

	UploadBufferResult CreateDeviceLocalBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage);

	// Image helpers
	UploadImageResult CreateDeviceLocalImage(
		uint32_t Width,
		uint32_t Height,
		vk::Format Format,
		uint32_t MipLevels,
		vk::ImageUsageFlags Usage);

	void GenerateMipChain(vk::raii::CommandBuffer& Cmd, vk::Image Image, uint32_t Width, uint32_t Height, uint32_t MipLevels);

	void GenerateNormalMipChain(vk::raii::CommandBuffer& Cmd, vk::Image Image, uint32_t Width, uint32_t Height, uint32_t MipLevels, const NormalMipResources& Resources);

	// Submit a one‑time command buffer and wait for completion
	void SubmitCopy(std::function<void(vk::raii::CommandBuffer&)> RecordCommands);

	// Submit a one‑time command buffer for graphics work and wait for completion
	void SubmitGraphicsWork(std::function<void(vk::raii::CommandBuffer&)> RecordCommands);
	
	std::unique_ptr<NormalMipResources> CreateNormalMipResources(vk::Image Image, vk::Format Format, uint32_t MipLevels);

	void ValidateNormalMipUpload(const ImageUploadInfo& Info) const;

	const vk::raii::Device& Device;
	const vk::raii::PhysicalDevice& PhysicalDevice;
	const vk::raii::Queue& TransferQueue;
	const vk::raii::Queue& GraphicsQueue;
	uint32_t TransferQueueFamilyIndex;
	uint32_t GraphicsQueueFamilyIndex;
	bool bSameQueueFamily;

	vk::raii::CommandPool TransferCommandPool = nullptr;
	vk::raii::CommandPool GraphicsCommandPool = nullptr;
	vk::raii::Fence UploadFence = nullptr;

	// Normal map mip generation pipeline and layout
	inline static constexpr vk::Format NormalMipFormat = vk::Format::eR8G8B8A8Unorm;

	vk::raii::DescriptorSetLayout NormalMipDescriptorSetLayout = nullptr;
	vk::Pipeline NormalMipPipeline = nullptr;
	vk::PipelineLayout NormalMipPipelineLayout = nullptr;
	bool bNormalMipGenerationReady = false;
};
