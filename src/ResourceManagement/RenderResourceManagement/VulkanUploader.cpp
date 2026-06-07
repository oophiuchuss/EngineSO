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

VulkanUploader::UploadResult VulkanUploader::Upload(const void* Data, vk::DeviceSize Size, vk::BufferUsageFlags TargetUsage)
{
	// Create staging buffer in host-visible memory and copy data to it
	StagingBuffer Staging = CreateStagingBuffer(Data, Size);

	// Create destination buffer in device-local memory
	// eTransferDst allows it to be the destination of a transfer operation
	UploadResult Result = CreateDeviceLocalBuffer(Size, TargetUsage | vk::BufferUsageFlagBits::eTransferDst);

	// Record and submit copy from staging to device-local buffer
	SubmitCopy(*Staging.Buffer, *Result.Buffer, Size);

	// Stating buffer destroyed - RAII cleans up automatically
	return Result;
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

VulkanUploader::UploadResult VulkanUploader::CreateDeviceLocalBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage)
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

void VulkanUploader::SubmitCopy(vk::Buffer Src, vk::Buffer Dst, vk::DeviceSize Size)
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

	vk::BufferCopy CopyRegion(0, 0, Size);
	Cmd.copyBuffer(Src, Dst, CopyRegion);

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
