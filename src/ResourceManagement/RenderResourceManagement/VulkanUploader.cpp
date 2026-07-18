module;

#include <vulkan/vulkan_raii.hpp>
#include <stdexcept>

module VulkanUploader;

import VulkanUtils;
import PushConstants;

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

		vk::ImageUsageFlags Usage =
			vk::ImageUsageFlagBits::eTransferDst |
			vk::ImageUsageFlagBits::eSampled;

		if (Info.MipGeneration == VulkanUploader::ImageMipGeneration::NormalMapCompute)
		{
			ValidateNormalMipUpload(Info);
			Usage |= vk::ImageUsageFlagBits::eStorage;
		}
		else
		{
			Usage |= vk::ImageUsageFlagBits::eTransferSrc;
		}

		Stagings.push_back(CreateStagingBuffer(Info.PixelData, DataSize));
		Results.push_back(CreateDeviceLocalImage(Info.Width, Info.Height, Info.Format, MipLevelCount, Usage));
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

	std::vector<std::unique_ptr<NormalMipResources>>
		NormalResources(Images.size());

	for (size_t i = 0; i < Images.size(); ++i)
	{
		if (Images[i].MipGeneration == ImageMipGeneration::NormalMapCompute)
		{
			NormalResources[i] = CreateNormalMipResources(*Results[i].Image, Images[i].Format, Results[i].MipLevels);
		}
	}

	SubmitGraphicsWork([&](vk::raii::CommandBuffer& Cmd)
		{
			for (size_t i = 0; i < Images.size(); ++i)
			{
				if (Images[i].MipGeneration == ImageMipGeneration::NormalMapCompute)
				{
					GenerateNormalMipChain(Cmd, *Results[i].Image, Images[i].Width, Images[i].Height, Results[i].MipLevels, *NormalResources[i]);
				}
				else
				{
					GenerateMipChain(Cmd, *Results[i].Image, Images[i].Width, Images[i].Height, Results[i].MipLevels);
				}
			}
		});

	// Staging buffers go out of scope and are freed.
	// The created device‑local images are returned to the caller.
	return Results;
}

void VulkanUploader::InitializeNormalMipGeneration(Shader& InShader, PipelineCache& InPipelineCache)
{
	vk::FormatProperties Properties = PhysicalDevice.getFormatProperties(NormalMipFormat);

	const vk::FormatFeatureFlags RequiredFeatures = vk::FormatFeatureFlagBits::eStorageImage;

	if ((Properties.optimalTilingFeatures & RequiredFeatures) != RequiredFeatures)
	{
		throw std::runtime_error("R8G8B8A8_UNORM does not support storage images");
	}

	if (InShader.GetProgramType() !=ShaderProgramType::Compute)
	{
		throw std::invalid_argument("Normal mip generation requires a compute shader");
	}

	if (NormalMipDescriptorSetLayout != nullptr)
	{
		throw std::logic_error("Normal mip generation was already initialized");
	}

	std::array<vk::DescriptorSetLayoutBinding, 2> Bindings = { {
			{
				0,
				vk::DescriptorType::eSampledImage,
				1,
				vk::ShaderStageFlagBits::eCompute
			},
			{
				1,
				vk::DescriptorType::eStorageImage,
				1,
				vk::ShaderStageFlagBits::eCompute
			}
		} };

	vk::DescriptorSetLayoutCreateInfo LayoutInfo(
		{},
		Bindings);

	NormalMipDescriptorSetLayout = Device.createDescriptorSetLayout(LayoutInfo);

	ComputePipelineKey Key;
	Key.ShaderPtr = &InShader;
	Key.DescriptorSetLayouts = { *NormalMipDescriptorSetLayout};
	Key.PushConstantRange = vk::PushConstantRange(
		vk::ShaderStageFlagBits::eCompute,
		0,
		sizeof(NormalMipPushConstants)
	);

	PipelineHandles Handles = InPipelineCache.GetOrCreateCompute(Key);

	if (Handles.Pipeline == nullptr || Handles.Layout == nullptr)
	{
		throw std::runtime_error("Failed to initialize normal mip pipeline");
	}

	NormalMipPipeline = Handles.Pipeline;
	NormalMipPipelineLayout = Handles.Layout;
	bNormalMipGenerationReady = true;
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
	uint32_t MipLevels,
	vk::ImageUsageFlags Usage)
{
	std::array<uint32_t, 2> QueueFamilies = { TransferQueueFamilyIndex, GraphicsQueueFamilyIndex };
	bool bNeedsConcurrent = !bSameQueueFamily;

	vk::ImageCreateInfo ImageInfo(
		{}, vk::ImageType::e2D, Format,
		vk::Extent3D(Width, Height, 1),
		MipLevels, 1,
		vk::SampleCountFlagBits::e1,
		vk::ImageTiling::eOptimal,
		Usage,
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

void VulkanUploader::GenerateNormalMipChain(vk::raii::CommandBuffer& Cmd, vk::Image Image, uint32_t Width, uint32_t Height, uint32_t MipLevels, const NormalMipResources& Resources)
{
	if (MipLevels <= 1)
	{
		VulkanUtils::TransitionImageLayout(
			Cmd, Image, vk::ImageAspectFlagBits::eColor,
			vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
			0, 1);

		return;
	}

	if (Resources.DescriptorSets.size() != MipLevels - 1)
	{
		throw std::runtime_error("Normal mip descriptor count mismatch");
	}

	Cmd.bindPipeline(vk::PipelineBindPoint::eCompute, NormalMipPipeline);

	uint32_t SourceWidth = Width;
	uint32_t SourceHeight = Height;

	for (uint32_t Level = 1; Level < MipLevels; ++Level)
	{
		const uint32_t DestinationWidth = (std::max)(SourceWidth / 2, 1u);

		const uint32_t DestinationHeight = (std::max)(SourceHeight / 2, 1u);

		// Mip zero was filled by the transfer upload.
		if (Level == 1)
		{
			VulkanUtils::TransitionImageLayout(
				Cmd, Image, vk::ImageAspectFlagBits::eColor,
				vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::PipelineStageFlagBits::eTransfer, 
				vk::PipelineStageFlagBits::eComputeShader |
				vk::PipelineStageFlagBits::eFragmentShader,
				vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
				0, 1);
		}

		// This mip has not been used yet. Prepare it for storage writes.
		VulkanUtils::TransitionImageLayout(
			Cmd, Image, vk::ImageAspectFlagBits::eColor,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
			vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader,
			vk::AccessFlagBits::eNone, vk::AccessFlagBits::eShaderWrite,
			Level, 1);

		std::array<vk::DescriptorSet, 1> DescriptorSets = {
			*Resources.DescriptorSets[Level - 1]
		};

		Cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eCompute,
			NormalMipPipelineLayout,
			0,
			DescriptorSets,
			{});

		NormalMipPushConstants Push = {
			SourceWidth,
			SourceHeight,
			DestinationWidth,
			DestinationHeight
		};

		Cmd.pushConstants<NormalMipPushConstants>(
			NormalMipPipelineLayout,
			vk::ShaderStageFlagBits::eCompute,
			0,
			Push);

		constexpr uint32_t GroupSize = 8;

		const uint32_t GroupCountX = (DestinationWidth + GroupSize - 1) / GroupSize;

		const uint32_t GroupCountY = (DestinationHeight + GroupSize - 1) / GroupSize;

		Cmd.dispatch(GroupCountX, GroupCountY, 1);

		// Make this write available to:
		// 1. the next compute dispatch, where it becomes the source;
		// 2. later fragment-shader texture sampling.
		VulkanUtils::TransitionImageLayout(
			Cmd, Image, vk::ImageAspectFlagBits::eColor,
			vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader |
			vk::PipelineStageFlagBits::eFragmentShader,
			vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead,
			Level, 1);

		SourceWidth = DestinationWidth;
		SourceHeight = DestinationHeight;
	}
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

std::unique_ptr<VulkanUploader::NormalMipResources> VulkanUploader::CreateNormalMipResources(vk::Image Image, vk::Format Format, uint32_t MipLevels)
{
	auto Resources = std::make_unique<NormalMipResources>();
	if (MipLevels <= 1)
	{
		return Resources;
	}

	Resources->MipViews.reserve(MipLevels);

	for (uint32_t Level = 0; Level < MipLevels; ++Level)
	{
		vk::ImageViewCreateInfo ViewInfo(
			{},
			Image,
			vk::ImageViewType::e2D,
			Format,
			{},
			{ vk::ImageAspectFlagBits::eColor, Level, 1, 0, 1 });

		Resources->MipViews.push_back(Device.createImageView(ViewInfo));
	}

	const uint32_t GeneratedMipCount = MipLevels - 1;

	std::array<vk::DescriptorPoolSize, 2> PoolSizes = { {
		{
			vk::DescriptorType::eSampledImage,
			GeneratedMipCount
		},
		{
			vk::DescriptorType::eStorageImage,
			GeneratedMipCount
		}
	} };

	vk::DescriptorPoolCreateInfo PoolInfo(
		vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		GeneratedMipCount,
		PoolSizes);

	Resources->DescriptorPool = Device.createDescriptorPool(PoolInfo);

	std::vector<vk::DescriptorSetLayout> Layouts(
		GeneratedMipCount,
		*NormalMipDescriptorSetLayout);

	vk::DescriptorSetAllocateInfo AllocateInfo(
		*Resources->DescriptorPool,
		Layouts);

	Resources->DescriptorSets = Device.allocateDescriptorSets(AllocateInfo);

	for (uint32_t Level = 1; Level < MipLevels; Level++)
	{
		vk::DescriptorImageInfo SourceInfo(
			{},
			*Resources->MipViews[Level - 1],
			vk::ImageLayout::eShaderReadOnlyOptimal);

		vk::DescriptorImageInfo DestinationInfo(
			{},
			*Resources->MipViews[Level],
			vk::ImageLayout::eGeneral);

		std::array<vk::WriteDescriptorSet, 2> Writes = { {
			{
				*Resources->DescriptorSets[Level - 1],
				0,
				0,
				1,
				vk::DescriptorType::eSampledImage,
				&SourceInfo
			},
			{
				*Resources->DescriptorSets[Level - 1],
				1,
				0,
				1,
				vk::DescriptorType::eStorageImage,
				&DestinationInfo
			}
		} };

		Device.updateDescriptorSets(Writes, {});
	}

	return Resources;
}

void VulkanUploader::ValidateNormalMipUpload(const ImageUploadInfo& Info) const
{
	if (!bNormalMipGenerationReady)
	{
		throw std::runtime_error("Normal mip generation is not initialized");
	}

	if (Info.Format != NormalMipFormat)
	{
		throw std::runtime_error("Normal mip generation requires R8G8B8A8_UNORM");
	}
}
