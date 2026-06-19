module;

#include <vulkan/vulkan_raii.hpp>
#include <stdexcept>

module VulkanUploader;

import VulkanUtils;

VulkanUploader::VulkanUploader(
	const vk::raii::Device& Device, 
	const vk::raii::PhysicalDevice& PhysicalDevice, 
	uint32_t TransferQueueFamilyIndex, 
	uint32_t GraphicsQueueFamilyIndex, 
	const vk::raii::Queue& TransferQueue) : 
	Device(Device),
	PhysicalDevice(PhysicalDevice),
	TransferQueue(TransferQueue),
	TransferQueueFamilyIndex(TransferQueueFamilyIndex),
	GraphicsQueueFamilyIndex(GraphicsQueueFamilyIndex),
	bSameQueueFamily(TransferQueueFamilyIndex == GraphicsQueueFamilyIndex)
{
	// Create command pool for transfer operations
	// eTransient hints driver these buffers are short-lived
	vk::CommandPoolCreateInfo PoolInfo(
		vk::CommandPoolCreateFlagBits::eTransient,
		TransferQueueFamilyIndex);

	TransferCommandPool = Device.createCommandPool(PoolInfo);

	// Reusable fence for synchronizing uploads
	UploadFence = Device.createFence(vk::FenceCreateFlags{});
}

VulkanUploader::UploadBufferResult VulkanUploader::UploadBuffer(const void* Data, vk::DeviceSize Size, vk::BufferUsageFlags TargetUsage)
{
	// Create staging buffer in host-visible memory and copy data to it
	StagingBuffer Staging = CreateStagingBuffer(Data, Size);

	// Create destination buffer in device-local memory
	// eTransferDst allows it to be the destination of a transfer operation
	UploadBufferResult Result = CreateDeviceLocalBuffer(Size, TargetUsage | vk::BufferUsageFlagBits::eTransferDst);


	// Record and submit copy from staging to device-local buffer
	SubmitCopy([&](vk::raii::CommandBuffer& Cmd)
		{
			vk::BufferCopy CopyRegion(0, 0, Size);
			Cmd.copyBuffer(*Staging.Buffer, *Result.Buffer, CopyRegion);
		}
	);

	// Stating buffer destroyed - RAII cleans up automatically
	return Result;
}

VulkanUploader::UploadImageResult VulkanUploader::UploadImage(const void* PixelData, uint32_t Width, uint32_t Height, vk::Format Format)
{
	// Stage the raw pixel data
	vk::DeviceSize DataSize = Width * Height * 4;   // TODO: RGBA for now. Should be changed in the future
	StagingBuffer Staging = CreateStagingBuffer(PixelData, DataSize);

	// Create the target device‑local image
	UploadImageResult Result = CreateDeviceLocalImage(Width, Height, Format);

	// Record and submit the copy + layout transitions
	SubmitCopy([&](vk::raii::CommandBuffer& Cmd)
		{
			// First transition - from undefined to transfer dst (transfer queue compatible)
			VulkanUtils::TransitionImageLayout(
				Cmd, *Result.Image,
				vk::ImageAspectFlagBits::eColor,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eTransferDstOptimal);

			vk::BufferImageCopy Region(
				0, 0, 0,
				{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
				{ 0, 0, 0 },
				{ Width, Height, 1 });

			Cmd.copyBufferToImage(
				*Staging.Buffer, *Result.Image,
				vk::ImageLayout::eTransferDstOptimal, Region);

			// Second transition - explicit, transfer queue doesn't support eFragmentShader
			VulkanUtils::TransitionImageLayout(
				Cmd, *Result.Image,
				vk::ImageAspectFlagBits::eColor,
				vk::ImageLayout::eTransferDstOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eNone);
		});

	return Result;
}

std::vector<VulkanUploader::UploadImageResult> VulkanUploader::UploadImageBatch(
	std::span<const ImageUploadInfo> Images)
{
	std::vector<UploadImageResult> Results;
	if (Images.empty())
		return Results;

	// Create staging buffers and device‑local images for every entry.
	std::vector<StagingBuffer> Stagings;
	Stagings.reserve(Images.size());
	Results.reserve(Images.size());

	for (const auto& Info : Images)
	{
		vk::DeviceSize DataSize = Info.Width * Info.Height * 4;   // TODO: for now always RGBA
		Stagings.push_back(CreateStagingBuffer(Info.PixelData, DataSize));
		Results.push_back(CreateDeviceLocalImage(Info.Width, Info.Height, Info.Format));
	}

	// Record everything into one command buffer.
	   // 2. Record all copies into one command buffer, submit once, wait once.
	SubmitCopy([&](vk::raii::CommandBuffer& Cmd)
		{
			for (size_t i = 0; i < Images.size(); ++i)
			{
				VulkanUtils::TransitionImageLayout(
					Cmd, *Results[i].Image,
					vk::ImageAspectFlagBits::eColor,
					vk::ImageLayout::eUndefined,
					vk::ImageLayout::eTransferDstOptimal);

				vk::BufferImageCopy Region(
					0, 0, 0,
					{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
					{ 0, 0, 0 },
					{ Images[i].Width, Images[i].Height, 1 });

				Cmd.copyBufferToImage(
					*Stagings[i].Buffer,
					*Results[i].Image,
					vk::ImageLayout::eTransferDstOptimal,
					Region);

				VulkanUtils::TransitionImageLayout(
					Cmd, *Results[i].Image,
					vk::ImageAspectFlagBits::eColor,
					vk::ImageLayout::eTransferDstOptimal,
					vk::ImageLayout::eShaderReadOnlyOptimal,
					vk::PipelineStageFlagBits::eTransfer,
					vk::PipelineStageFlagBits::eBottomOfPipe,
					vk::AccessFlagBits::eTransferWrite,
					vk::AccessFlagBits::eNone);
			}
		});

	// Staging buffers go out of scope and are freed.
	// The created device‑local images are returned to the caller.
	return Results;
}
VulkanUploader::StagingBuffer VulkanUploader::CreateStagingBuffer(const void* Data, vk::DeviceSize Size)
{
	vk::BufferCreateInfo BufferInfo(
		{},
		Size,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::SharingMode::eExclusive
	);

	vk::raii::Buffer Buffer = Device.createBuffer(BufferInfo);
	auto MemReqs = Buffer.getMemoryRequirements();

	uint32_t MemTypeIndex = VulkanUtils::FindMemoryType(
		PhysicalDevice,
		MemReqs.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent);

	vk::raii::DeviceMemory Memory = Device.allocateMemory(
		vk::MemoryAllocateInfo(MemReqs.size, MemTypeIndex));

	Buffer.bindMemory(*Memory, 0);

	// Copy data to staging buffer
	void* Mapped = Memory.mapMemory(0, Size);
	std::memcpy(Mapped, Data, static_cast<size_t>(Size));
	Memory.unmapMemory();

	return {std::move(Buffer), std::move(Memory)};
}

VulkanUploader::UploadBufferResult VulkanUploader::CreateDeviceLocalBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage)
{
	// If different queue families the buffer needs to be accessible by both
	std::vector<uint32_t> QueueFamilies;
	vk::SharingMode SharingMode = vk::SharingMode::eExclusive;

	if (!bSameQueueFamily)
	{
		QueueFamilies = { TransferQueueFamilyIndex, GraphicsQueueFamilyIndex };
		SharingMode = vk::SharingMode::eConcurrent;
	}

	vk::BufferCreateInfo BufferInfo(
		{},
		Size,
		Usage,
		SharingMode,
		QueueFamilies);

	vk::raii::Buffer Buffer = Device.createBuffer(BufferInfo);
	auto MemReqs = Buffer.getMemoryRequirements();

	// Device local — GPU can access fast, CPU cannot
	uint32_t MemTypeIndex = VulkanUtils::FindMemoryType(
		PhysicalDevice,
		MemReqs.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::raii::DeviceMemory Memory = Device.allocateMemory(
		vk::MemoryAllocateInfo(MemReqs.size, MemTypeIndex));

	Buffer.bindMemory(*Memory, 0);

	return { std::move(Buffer), std::move(Memory) };
}

VulkanUploader::UploadImageResult VulkanUploader::CreateDeviceLocalImage(uint32_t Width, uint32_t Height, vk::Format Format)
{
	vk::ImageCreateInfo ImageInfo(
		{}, vk::ImageType::e2D, Format,
		vk::Extent3D(Width, Height, 1),
		1, 1,
		vk::SampleCountFlagBits::e1,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::SharingMode::eExclusive);

	vk::raii::Image Image = Device.createImage(ImageInfo);
	auto MemReqs = Image.getMemoryRequirements();

	uint32_t MemTypeIdx = VulkanUtils::FindMemoryType(
		PhysicalDevice,
		MemReqs.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::raii::DeviceMemory Memory = Device.allocateMemory(
		vk::MemoryAllocateInfo(MemReqs.size, MemTypeIdx));

	Image.bindMemory(*Memory, 0);

	return { std::move(Image), std::move(Memory) };
}


void VulkanUploader::SubmitCopy(std::function<void(vk::raii::CommandBuffer&)> RecordCommands)
{
	// Allocate temporary command buffer for this upload
	vk::CommandBufferAllocateInfo AllocInfo(
		*TransferCommandPool,
		vk::CommandBufferLevel::ePrimary,
		1);

	auto CmdBuffers = Device.allocateCommandBuffers(AllocInfo);
	auto& Cmd = CmdBuffers.front();

	// Record copy command
	Cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	RecordCommands(Cmd);

	// If different families — release ownership from transfer queue
	// Graphics queue will implicitly acquire since we used eConcurrent sharing
	// For truly exclusive sharing this would need a separate acquire barrier
	// on the graphics queue, but eConcurrent handles it correctly here

	Cmd.end();

	// Submit to tranfer queue and wait for completion
	vk::SubmitInfo SubmitInfo;
	SubmitInfo.setCommandBufferCount(1);
	SubmitInfo.setPCommandBuffers(&*Cmd);

	// Reset fence before using
	Device.resetFences(*UploadFence);

	TransferQueue.submit(SubmitInfo, *UploadFence);

	// Wait for transfer to complete before returning
	// TODO: maybe to batch multiple uploads together and wait on a single fence later
	auto WaitResult = Device.waitForFences(*UploadFence, VK_TRUE, UINT64_MAX);

	if (WaitResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to wait for upload fence");
	}

	// CmdBuffers goes out of scope here — RAII frees it automatically
}