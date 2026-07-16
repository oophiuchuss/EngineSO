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
	const vk::raii::Queue& TransferQueue,
	const vk::raii::Queue& GraphicsQueue) :
	Device(Device),
	PhysicalDevice(PhysicalDevice),
	TransferQueue(TransferQueue),
	GraphicsQueue(GraphicsQueue),
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

	// Create command pool for graphics operations
	// eTransient hints driver these buffers are short-lived
	vk::CommandPoolCreateInfo GraphicsPoolInfo(
		vk::CommandPoolCreateFlagBits::eTransient,
		GraphicsQueueFamilyIndex);

	GraphicsCommandPool = Device.createCommandPool(GraphicsPoolInfo);

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

VulkanUploader::UploadImageResult VulkanUploader::UploadImage(const ImageUploadInfo& Image)
{
	auto Results = UploadImageBatch(std::span<const ImageUploadInfo>(&Image, 1));
	if (Results.empty())
	{
		throw std::runtime_error("UploadImageBatch returned no result for a single image");
	}
	return std::move(Results.front());
}

std::vector<VulkanUploader::UploadBufferResult> VulkanUploader::UploadBufferBatch(std::span<const BufferUploadInfo> Infos)
{
	std::vector<UploadBufferResult> Results;
	if (Infos.empty()) return Results;

	// Create staging buffers and device‑local buffers for all entries
	std::vector<StagingBuffer> Stagings;
	Stagings.reserve(Infos.size());
	Results.reserve(Infos.size());

	for (const auto& Info : Infos)
	{
		Stagings.push_back(CreateStagingBuffer(Info.Data, Info.Size));
		Results.push_back(CreateDeviceLocalBuffer(Info.Size, Info.TargetUsage | vk::BufferUsageFlagBits::eTransferDst));
	}

	// Record all copies into one command buffer
	SubmitCopy([&](vk::raii::CommandBuffer& Cmd)
		{
			for (size_t i = 0; i < Infos.size(); ++i)
			{
				vk::BufferCopy CopyRegion(0, 0, Infos[i].Size);
				Cmd.copyBuffer(*Stagings[i].Buffer, *Results[i].Buffer, CopyRegion);
			}
		});

	return Results;
}

std::vector<VulkanUploader::UploadImageResult> VulkanUploader::UploadImageBatch(std::span<const ImageUploadInfo> Images)
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
		vk::DeviceSize DataSize = static_cast<vk::DeviceSize>(Info.Width) * Info.Height * 4;
		uint32_t MipLevelCount = VulkanUtils::ComputeMipLevels(Info.Width, Info.Height);

		Stagings.push_back(CreateStagingBuffer(Info.PixelData, DataSize));
		Results.push_back(CreateDeviceLocalImage(Info.Width, Info.Height, Info.Format, MipLevelCount));
	}

	// Image uploads stay on the graphics queue because mip generation and final
	// shader-read transitions also execute there. This keeps the transfer writes
	// and their consuming barriers on one queue and avoids an otherwise-required
	// cross-queue semaphore dependency.
	SubmitGraphicsWork([&](vk::raii::CommandBuffer& Cmd)
		{
			for (size_t i = 0; i < Images.size(); ++i)
			{
				VulkanUtils::TransitionImageLayout(
					Cmd, *Results[i].Image,
					vk::ImageAspectFlagBits::eColor,
					vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
					vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
					vk::AccessFlagBits::eNone, vk::AccessFlagBits::eTransferWrite,
					0, 1);

				vk::BufferImageCopy Region(
					0, 0, 0,
					vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
					vk::Offset3D(0, 0, 0),
					vk::Extent3D(Images[i].Width, Images[i].Height, 1));

				Cmd.copyBufferToImage(
					*Stagings[i].Buffer,
					*Results[i].Image,
					vk::ImageLayout::eTransferDstOptimal,
					Region);
			}
		});

	SubmitGraphicsWork([&](vk::raii::CommandBuffer& Cmd)
		{
			for (size_t i = 0; i < Images.size(); ++i)
			{
				GenerateMipChain(Cmd, *Results[i].Image, Images[i].Width, Images[i].Height, Results[i].MipLevels);
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

VulkanUploader::UploadImageResult VulkanUploader::CreateDeviceLocalImage(
	uint32_t Width,
	uint32_t Height,
	vk::Format Format,
	uint32_t MipLevels)
{
	std::array<uint32_t, 2> QueueFamilies = { TransferQueueFamilyIndex, GraphicsQueueFamilyIndex };
	bool bNeedsConcurrent = !bSameQueueFamily;

	vk::ImageCreateInfo ImageInfo(
		{}, vk::ImageType::e2D, Format,
		vk::Extent3D(Width, Height, 1),
		MipLevels, 1,
		vk::SampleCountFlagBits::e1,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst |
		vk::ImageUsageFlagBits::eTransferSrc |	// for generating mipmaps
		vk::ImageUsageFlagBits::eSampled,
		bNeedsConcurrent ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
		bNeedsConcurrent ? static_cast<uint32_t>(QueueFamilies.size()) : 0,
		bNeedsConcurrent ? QueueFamilies.data() : nullptr);

	vk::raii::Image Image = Device.createImage(ImageInfo);
	auto MemReqs = Image.getMemoryRequirements();

	uint32_t MemTypeIdx = VulkanUtils::FindMemoryType(
		PhysicalDevice,
		MemReqs.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::raii::DeviceMemory Memory = Device.allocateMemory(
		vk::MemoryAllocateInfo(MemReqs.size, MemTypeIdx));

	Image.bindMemory(*Memory, 0);

	return { std::move(Image), std::move(Memory), MipLevels };
}

void VulkanUploader::GenerateMipChain(vk::raii::CommandBuffer& Cmd, vk::Image Image, uint32_t Width, uint32_t Height, uint32_t MipLevels)
{
	int32_t MipWidth = static_cast<int32_t>(Width);
	int32_t MipHeight = static_cast<int32_t>(Height);

	for (uint32_t Level = 1; Level < MipLevels; ++Level)
	{
		VulkanUtils::TransitionImageLayout(
			Cmd, Image,
			vk::ImageAspectFlagBits::eColor,
			vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
			Level - 1, 1);

		VulkanUtils::TransitionImageLayout(
			Cmd, Image, vk::ImageAspectFlagBits::eColor,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
			vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
			vk::AccessFlagBits::eNone, vk::AccessFlagBits::eTransferWrite,
			Level, 1);

		int32_t NextWidth = (std::max)(MipWidth / 2, 1);
		int32_t NextHeight = (std::max)(MipHeight / 2, 1);

		vk::ImageBlit Blit;
		Blit.srcSubresource = { vk::ImageAspectFlagBits::eColor, Level - 1, 0, 1 };
		Blit.srcOffsets[0] = vk::Offset3D(0, 0, 0);
		Blit.srcOffsets[1] = vk::Offset3D(MipWidth, MipHeight, 1);
		Blit.dstSubresource = { vk::ImageAspectFlagBits::eColor, Level, 0, 1 };
		Blit.dstOffsets[0] = vk::Offset3D(0, 0, 0);
		Blit.dstOffsets[1] = vk::Offset3D(NextWidth, NextHeight, 1);

		Cmd.blitImage(
			Image, vk::ImageLayout::eTransferSrcOptimal,
			Image, vk::ImageLayout::eTransferDstOptimal,
			Blit, vk::Filter::eLinear);

		VulkanUtils::TransitionImageLayout(
			Cmd, Image,
			vk::ImageAspectFlagBits::eColor,
			vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
			vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderRead,
			Level - 1, 1);

		MipWidth = NextWidth;
		MipHeight = NextHeight;
	}

	// Last level was only ever a blit destination, never read from — transition separately.
	VulkanUtils::TransitionImageLayout(
		Cmd, Image, vk::ImageAspectFlagBits::eColor,
		vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
		vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
		MipLevels - 1, 1);
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
	auto WaitResult = Device.waitForFences(*UploadFence, VK_TRUE, UINT64_MAX);

	if (WaitResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to wait for upload fence");
	}

	// CmdBuffers goes out of scope here — RAII frees it automatically
}

void VulkanUploader::SubmitGraphicsWork(std::function<void(vk::raii::CommandBuffer&)> RecordCommands)
{
	// Allocate temporary command buffer for this work
	vk::CommandBufferAllocateInfo AllocInfo(
		*GraphicsCommandPool,
		vk::CommandBufferLevel::ePrimary,
		1);

	auto CmdBuffers = Device.allocateCommandBuffers(AllocInfo);
	auto& Cmd = CmdBuffers.front();

	// Record graphics commands
	Cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	RecordCommands(Cmd);

	Cmd.end();

	vk::SubmitInfo SubmitInfo;
	SubmitInfo.setCommandBufferCount(1);
	SubmitInfo.setPCommandBuffers(&*Cmd);

	// Reset fence before using
	Device.resetFences(*UploadFence);

	GraphicsQueue.submit(SubmitInfo, *UploadFence);

	// Wait for graphics work to complete before returning
	auto WaitResult = Device.waitForFences(*UploadFence, VK_TRUE, UINT64_MAX);
	if (WaitResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to wait for graphics upload fence");
	}
}
