module;

#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <optional>
#include <memory>
#include <iostream>

module Renderer;

import Rendergraph;
import CullingSystem;
import Entity;
import GeometryRenderPass;
import LightingPass;
import PostProcessPass;
import CameraUniform;

import TransformComponent; // TODO: maybe not the best idea to have tansform component coupled with the renderer, but for now it simplifies things

import Shader;	// TODO: For now, but should be per material
import Mesh;	// TODO: For now, but should be per material
import Geometry;

Renderer::Renderer(vk::raii::Instance& Instance, vk::raii::SurfaceKHR&& Surface) : Instance(Instance), Surface(std::move(Surface))
{
	// Pick physical device
	PickPhysicalDevice();
	// Create logical device and queues
	CreateLogicalDevice();
	// Create swapchain and related resources
	CreateSwapchain();
	// Create synchronization primitives
	CreateSyncObjects();

	RendergraphPtr = std::make_unique<Rendergraph>(Device, PhysicalDevice);
	CullingSystemPtr = std::make_unique<CullingSystem>();   // no camera 
	CameraUBO = std::make_unique<CameraUniformBuffer>(Device, PhysicalDevice);

	// Create command pool and buffers
	vk::CommandPoolCreateInfo PoolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, 
									   GraphicsQueueFamilyIndex);

	CommandPool = vk::raii::CommandPool(Device, PoolInfo);

	vk::CommandBufferAllocateInfo CmdAllocInfo(*CommandPool, vk::CommandBufferLevel::ePrimary, MAX_FRAMES_IN_FLIGHT);

	CommandBuffers = std::move(Device.allocateCommandBuffers(CmdAllocInfo));

	{
		// Load default shader for geometry pass (TODO: should be per material, but for now we just want to test the rendergraph)
		DefaultShader = Shader::CreateGraphicsShader(Device, "shaders/basic_geometry_vert.spv", "shaders/basic_geometry_frag.spv");

		vk::PipelineLayoutCreateInfo PipelineLayoutInfo({}, { *CameraUBO->GetDescriptorSetLayout() });
		DefaultPipelineLayout = vk::raii::PipelineLayout(Device, PipelineLayoutInfo);

		auto Stages = DefaultShader->GetVertexShaderStageInfo();

		vk::VertexInputBindingDescription BindingDesc(
			0, sizeof(Vertex));

		vk::VertexInputAttributeDescription AttribDesc(
			0, 0, vk::Format::eR32G32B32Sfloat, 0);

		vk::PipelineVertexInputStateCreateInfo VertexInputInfo({}, BindingDesc, AttribDesc);
		vk::PipelineInputAssemblyStateCreateInfo InputAssemblyInfo({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

		// Viewport and scissor will be dynamic states, so we don't specify them here
		vk::PipelineViewportStateCreateInfo ViewportStateInfo({}, 1, nullptr, 1, nullptr);

		vk::PipelineRasterizationStateCreateInfo RasterizerInfo(
			{}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill,
			vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise,
			VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);

		vk::PipelineMultisampleStateCreateInfo MultisampleInfo(
			{}, vk::SampleCountFlagBits::e1, VK_FALSE);

		vk::PipelineDepthStencilStateCreateInfo DepthStencilInfo(
			{}, VK_FALSE, VK_FALSE, vk::CompareOp::eLess,		 // TODO: disable for now
			VK_FALSE, VK_FALSE);

		vk::PipelineColorBlendAttachmentState ColorBlendAttachment;
		ColorBlendAttachment.setColorWriteMask(
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
		
		ColorBlendAttachment.setBlendEnable(VK_FALSE);

		vk::PipelineColorBlendStateCreateInfo ColorBlendInfo(
			{}, VK_FALSE, vk::LogicOp::eCopy, ColorBlendAttachment);

		vk::DynamicState DynamicStates[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		vk::PipelineDynamicStateCreateInfo DynamicStateInfo({}, DynamicStates);

		// Dynamic rendering info to render to the "Main_Color" and "Main_Depth" attachments defined in the rendergraph (these will be created as part of the rendergraph setup)
		const std::array<vk::Format, 1> ColorFormats = { vk::Format::eB8G8R8A8Unorm };
		vk::PipelineRenderingCreateInfo DynamicRenderingInfo(
			{},                         // viewMask (0)
			ColorFormats,               // colour attachment formats
			vk::Format::eD32Sfloat      // depth format
			// stencil format defaults to eUndefined, pNext defaults to nullptr
		);

		vk::GraphicsPipelineCreateInfo PipelineInfo(
			{},
			Stages, 
			&VertexInputInfo,
			&InputAssemblyInfo,
			nullptr,				// No tessellation
			&ViewportStateInfo,
			&RasterizerInfo, 
			&MultisampleInfo, 
			&DepthStencilInfo, 
			&ColorBlendInfo, 
			&DynamicStateInfo,
			DefaultPipelineLayout,
			nullptr,				// No render pass since we're using dynamic rendering
			0,						// Subpass
			nullptr,				// No base pipeline
			0);						// No base pipeline index

		PipelineInfo.setPNext(&DynamicRenderingInfo); // Chain dynamic rendering info to pipeline create info

		DefaultPipeline = Device.createGraphicsPipeline(nullptr, PipelineInfo);

		TesttriangleMesh = Mesh::CreateTestTriangle(Device, PhysicalDevice);
	}

	// Set up render passes and framebuffers in the rendergraph
	SetupRenderPasses();
	RendergraphPtr->Compile();
}

Renderer::~Renderer()
{
	if (Device != nullptr)
	{
		// vk::raii::Device is truthy if valid
		Device.waitIdle();
	}
}

void Renderer::RenderFrame(const std::vector<Entity*>& Entities)
{
	// Wait for previous frame to finish
	vk::Result FenceResult = Device.waitForFences({ *InFlightFences[CurrentFrame] }, VK_TRUE, UINT64_MAX);
	if (FenceResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to wait for in-flight fence");
	}

	// Acquire next image from swapchain
	vk::Result AcquireNextImageResult = vk::Result::eErrorUnknown;
	uint32_t ImageIndex;

	try {
		// Check for the feature test macro for the new API
#ifdef VULKAN_HPP_RAII_VERSION_1
		auto AcquireResult = Swapchain.acquireNextImage(UINT64_MAX, *ImageAvailableSemaphores[CurrentFrame]);
		AcquireNextImageResult = AcquireResult.result;
		ImageIndex = AcquireResult.value;
#else
		// Fallback for older versions returning std::pair
		std::tie(AcquireNextImageResult, ImageIndex) = Swapchain.acquireNextImage(UINT64_MAX, *ImageAvailableSemaphores[CurrentFrame]);
#endif
	}
	catch (const vk::OutOfDateKHRError&) {
		AcquireNextImageResult = vk::Result::eErrorOutOfDateKHR;
	}
	catch (const vk::SystemError& e) {
		throw;   // re‑throw other unexpected errors
	}
	if (AcquireNextImageResult == vk::Result::eErrorOutOfDateKHR) {
		RecreateSwapchain();
		return;	
	}
	else if (AcquireNextImageResult != vk::Result::eSuccess && AcquireNextImageResult != vk::Result::eSuboptimalKHR) {
		throw std::runtime_error("Failed to acquire swapchain image");
	}

	// Reset the fence for the current frame only after successfully acquiring an image, to avoid waiting on a fence that won't be signaled if acquisition fails
	Device.resetFences({ *InFlightFences[CurrentFrame] }); 

	// Reset command buffer before usage
	auto& Cmd = CommandBuffers[CurrentFrame];
	Cmd.reset(); // Reset command buffer for recording
	
	vk::CommandBufferBeginInfo BeginInfo({});
	Cmd.begin(BeginInfo); // TODO: should this command buffer begin with no additional info?
	
	if (CullingSystemPtr->GetActiveCamera())
	{
		CameraComponent* CamComp = CullingSystemPtr->GetActiveCamera();

		TransformComponent* CamTrans = CamComp->GetOwner()->GetComponent<TransformComponent>();

		CameraUniformData Data;
		Data.ViewProj = CamComp->GetViewMatrix() * CamComp->GetProjectionMatrix();
		Data.CameraPos = glm::vec4(CamTrans->GetPosition(), 1.0f);

		CameraUBO->Update(Data);
	}

	// Cull scene and update per-frame data
	CullingSystemPtr->CullScene(Entities);

	// TODO: make better handling of this
	GeometryRenderPass* GPass = dynamic_cast<GeometryRenderPass*>(RendergraphPtr->GetRenderPass("GeometryPass"));
	if (GPass)
	{
		GPass->SetRenderArea(SwapchainExtent);
	}

	// Record rendering commands into command buffer using rendergraph
	RendergraphPtr->Execute(Cmd, GraphicsQueue);

	// Copy "Main_Color" to swapchain image (transition layouts accordingly)
	Resource* MainColorResource = RendergraphPtr->GetResource("Main_Color");

	if (!MainColorResource || MainColorResource->Image == nullptr)
	{
		throw std::runtime_error("Main_Color resource not found in rendergraph");
	}

	vk::Image SrcImage = *MainColorResource->Image; // Get the Vulkan image handle from the resource

	// Transition the acquired swapchain image to be a transfer destination
	vk::ImageMemoryBarrier PreCopyBarrier(
		vk::AccessFlagBits::eNone,						// No previous access required (image was just acquired)
		vk::AccessFlagBits::eTransferWrite,				// Make it writable for transfer
		vk::ImageLayout::eUndefined,					// Current layout is undefined after acquire
		vk::ImageLayout::eTransferDstOptimal,			// Transition to transfer destination optimal for copy
		VK_QUEUE_FAMILY_IGNORED,						// No queue family ownership transfer
		VK_QUEUE_FAMILY_IGNORED,
		SwapchainImages[ImageIndex],					// The specific swapchain image we're targeting
		{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } // Full image, color aspect]
	);

	Cmd.pipelineBarrier(
		vk::PipelineStageFlagBits::eTopOfPipe,		// Earliest stage – we don't wait for anything
		vk::PipelineStageFlagBits::eTransfer,		// Before any transfer operations
		vk::DependencyFlagBits::eByRegion,			// Enable region-local optimization
		{}, {}, { PreCopyBarrier }
	);

	// Copy whole Main_Color image to swapchain image
	vk::ImageCopy CopyRegion;
	CopyRegion.setSrcSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })	// Source subresource (full image)
		.setDstSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })		// Destination subresource (full image)
		.setExtent(vk::Extent3D(MainColorResource->Extent, 1));					// Copy extent (full image size)
	
	Cmd.copyImage(
		SrcImage,						vk::ImageLayout::eTransferSrcOptimal,	// Source image and layout
		SwapchainImages[ImageIndex],	vk::ImageLayout::eTransferDstOptimal,	// Destination image and layout
		{ CopyRegion } // Regions to copy
	);

	// Transition swapchain image to present layout for presentation
	vk::ImageMemoryBarrier PresentBarrier(
		vk::AccessFlagBits::eTransferWrite,			// Wait for the copy to finish
		vk::AccessFlagBits::eNone,					// The presentation engine doesn't need any specific GPU access flags
		vk::ImageLayout::eTransferDstOptimal,       // Layout after copy is done
		vk::ImageLayout::ePresentSrcKHR,			// Target layout – required for presentation
		VK_QUEUE_FAMILY_IGNORED,                  // We're not transferring queue ownership
		VK_QUEUE_FAMILY_IGNORED,
		SwapchainImages[ImageIndex],              // The specific swapchain image we acquired
		{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }  // Full image, color aspect
	);

	// Execute the barrier
	Cmd.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,		// After transfer stage where copy happens
		vk::PipelineStageFlagBits::eBottomOfPipe,	// Latest stage – make sure layout change is done before anything later
		vk::DependencyFlagBits::eByRegion,			// Enable region-local optimization
		{}, {}, { PresentBarrier }
	);

	// Command submission with sync
	// Submit buffer
	Cmd.end();

	vk::PipelineStageFlags WaitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	vk::SubmitInfo SubmitInfo;
	SubmitInfo.setCommandBufferCount(1)		// Single command buffer
		.setPCommandBuffers(&*Cmd)			// Command buffer to exec
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(&*ImageAvailableSemaphores[CurrentFrame])
		.setPWaitDstStageMask(WaitStages)
		.setSignalSemaphoreCount(1)
		.setPSignalSemaphores(&*RenderFinishedSemaphores[ImageIndex]);


	GraphicsQueue.submit(SubmitInfo, *InFlightFences[CurrentFrame]);   // Submit to GPU queue

	// Present result
	vk::PresentInfoKHR PresentInfo;
	PresentInfo.setImageIndices(ImageIndex)
			   .setWaitSemaphoreCount(1)
			   .setPWaitSemaphores(&*RenderFinishedSemaphores[ImageIndex])
			   .setSwapchainCount(1)
			   .setPSwapchains(&*Swapchain);

	vk::Result PresentResult = vk::Result::eErrorUnknown;
	try {
		PresentResult = PresentQueue.presentKHR(PresentInfo);
	}
	catch (const vk::OutOfDateKHRError&) {
		PresentResult = vk::Result::eErrorOutOfDateKHR;
	}
	catch (const vk::SystemError& e) {
		throw;   // re‑throw other unexpected errors
	}

	if (PresentResult == vk::Result::eErrorOutOfDateKHR ||
		PresentResult == vk::Result::eSuboptimalKHR) {
		RecreateSwapchain();
	}
	else if (PresentResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to present swapchain image");
	}

	CurrentFrame = (CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT; // Advance to next frame index
}

void Renderer::RecreateSwapchain()
{
	// Handle minimization of window
	// Make sure that extent is not zero before proceeding due to perfomance
	auto SurfaceCaps = PhysicalDevice.getSurfaceCapabilitiesKHR(*Surface);

	if (SurfaceCaps.currentExtent.width == 0 ||
		SurfaceCaps.currentExtent.height == 0)
	{
		return; // Minimized window, skip
	}
	
	Device.waitIdle(); // Wait for device to be idle before recreating swapchain

	// Clean up old swapchain resources
	SwapchainImageViews.clear();
	Swapchain.clear(); // vk::raii::SwapchainKHR will automatically clean up old swapchain and related resources when reassigned

	// Create new swapchain with updated dimensions
	CreateSwapchain();

	// Recreate framebuffers and render passes that depend on swapchain images 
	RendergraphPtr->Reset(); // Clear existing rendergraph resources and passes
	SetupRenderPasses();
	RendergraphPtr->Compile();
	CreateSyncObjects(); // Recreate synchronization objects if they depend on swapchain (e.g. semaphores for each swapchain image)
}

void Renderer::SetActiveCamera(CameraComponent* Camera)
{
	CullingSystemPtr->SetActiveCamera(Camera);

	// Update camera component aspect ratio to match swapchain extent
	if (Camera)
	{
		float AspectRatio = static_cast<float>(SwapchainExtent.width) / static_cast<float>(SwapchainExtent.height);
		Camera->SetPerspective(Camera->GetFieldOfView(), AspectRatio, Camera->GetNearPlane(), Camera->GetFarPlane());
	}
}

void Renderer::PickPhysicalDevice()
{
	vk::raii::PhysicalDevices Devices(Instance);
	if (Devices.empty())
	{
		throw std::runtime_error("No Vulkan-compatible GPU found");
	}

	// Score devices and pick the best one
	int BestScore = -1;
	vk::raii::PhysicalDevice BestDevice = nullptr;

	for (auto& Dev : Devices)
	{
		// Must support present and graphics family

		auto QueueFamilies = Dev.getQueueFamilyProperties();
		bool bHasGraphics = false;
		bool bHasPresent = false;

		for (uint32_t i = 0; i < QueueFamilies.size(); i++)
		{
			if(QueueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
			{
				bHasGraphics = true;
			}
			
			if(Dev.getSurfaceSupportKHR(i, *Surface))
			{
				bHasPresent = true;
			}

			if(bHasGraphics && bHasPresent)
			{
				break;
			}
		}

		if (!bHasGraphics || !bHasPresent)
		{
			continue; // Skip devices that don't support required queue families
		}

		int Score = ScorePhysicalDevice(Dev, Surface);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestDevice = Dev;
		}
	}

	if(BestDevice == nullptr)
	{
		throw std::runtime_error("No suitable GPU found");
	}

	PhysicalDevice = BestDevice;
}

void Renderer::CreateLogicalDevice()
{
	// Find queue family indices (graphics and present)

	auto QueueFamilies = PhysicalDevice.getQueueFamilyProperties();
	
	auto FindGraphicsAndPresent = [&]() -> std::optional<uint32_t>
	{
		for (uint32_t i = 0; i < QueueFamilies.size(); i++)
		{
			if(QueueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics && PhysicalDevice.getSurfaceSupportKHR(i, *Surface))
			{
				return i; // Found a queue family that supports both graphics and present
			}
		}
		return {};
	};

	auto FindTransfer = [&]() -> std::optional<uint32_t>
	{
		for (uint32_t i = 0; i < QueueFamilies.size(); i++)
		{
			if(QueueFamilies[i].queueFlags & vk::QueueFlagBits::eTransfer && !(QueueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics))
			{
				return i; // Found a dedicated transfer queue family
			}
		}
		// fallback: if no dedicated transfer queue, use graphics queue
		for (uint32_t i = 0; i < QueueFamilies.size(); i++)
		{
			if(QueueFamilies[i].queueFlags & vk::QueueFlagBits::eTransfer)
			{
				return i; // Found a transfer queue family (may also support graphics)
			}
		}
		return {};
	};

	auto GraphicsAndPresentFamily = FindGraphicsAndPresent();
	auto TransferFamily = FindTransfer();

	if (!GraphicsAndPresentFamily.has_value())
	{
		throw std::runtime_error("Failed to find a queue family that supports both graphics and present");
	}

	std::vector<const char*> DeviceExtensions = {
	   VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	   VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
	};

	// Create logical device with required queues
	std::vector<vk::DeviceQueueCreateInfo> QueueCreateInfos;
	float QueuePriority = 1.0f;

	// Graphics and present queue (one queue for both)
	vk::DeviceQueueCreateInfo GraphicsQueueCreateInfo({}, GraphicsAndPresentFamily.value(), 1, &QueuePriority);
	QueueCreateInfos.push_back(GraphicsQueueCreateInfo);

	// Dedicated transfer queue
	if (TransferFamily.has_value() && TransferFamily.value() != GraphicsAndPresentFamily.value())
	{
		vk::DeviceQueueCreateInfo TransferQueueCreateInfo({}, TransferFamily.value(), 1, &QueuePriority);
		QueueCreateInfos.push_back(TransferQueueCreateInfo);
	}

	vk::DeviceCreateInfo DeviceCreateInfo({}, QueueCreateInfos, {}, DeviceExtensions);

	// Enable dynamic rendering feature
	vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature;
	dynamicRenderingFeature.setDynamicRendering(VK_TRUE);

	// Chain it into the device create info
	DeviceCreateInfo.setPNext(&dynamicRenderingFeature);

	Device = vk::raii::Device(PhysicalDevice, DeviceCreateInfo);

	GraphicsQueueFamilyIndex = GraphicsAndPresentFamily.value();

	// Get queue handles
	GraphicsQueue = Device.getQueue(GraphicsAndPresentFamily.value(), 0);
	PresentQueue = Device.getQueue(GraphicsAndPresentFamily.value(), 0); // Same queue for present
	if (TransferFamily.has_value() && TransferFamily.value() != GraphicsAndPresentFamily.value())
	{
		TransferQueue = Device.getQueue(*TransferFamily, 0);
		TransferQueueFamilyIndex = TransferFamily.value();
	}
	else
	{
		TransferQueue = GraphicsQueue; // Fallback: Use graphics queue for transfer if no dedicated transfer queue
		TransferQueueFamilyIndex = GraphicsAndPresentFamily.value();
	}
}

void Renderer::CreateSwapchain()
{
	auto SurfaceCaps = PhysicalDevice.getSurfaceCapabilitiesKHR(*Surface);
	auto SurfaceFormats = PhysicalDevice.getSurfaceFormatsKHR(*Surface);
	auto PresentModes = PhysicalDevice.getSurfacePresentModesKHR(*Surface);

	// Choose format that works even with overlay injections
	//
	// Overlays (RTSS, Steam, Discord, etc.) often hook into swapchain creation
	// and silently add VK_IMAGE_USAGE_STORAGE_BIT and VK_IMAGE_USAGE_TRANSFER_SRC_BIT
	// to the image usage. This causes a validation error (or even a crash) if the
	// chosen format does not support those extra flags.
	//
	// To avoid this, we ask the GPU what formats support our required usage PLUS
	// the extra usage that overlays typically add. If we find one, we use it.
	// Otherwise we fall back to a format that supports only our required usage.
	//
	// The visual output will always be correct sRGB because we pair the format
	// with VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, which tells the display hardware
	// to apply the sRGB gamma curve during presentation.

	vk::Format        SurfaceFormat = vk::Format::eUndefined;
	vk::ColorSpaceKHR SurfaceColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;

	vk::ImageUsageFlags RequiredUsage = vk::ImageUsageFlagBits::eColorAttachment
		| vk::ImageUsageFlagBits::eTransferDst;

	vk::ImageUsageFlags ExtraUsage = vk::ImageUsageFlagBits::eStorage
		| vk::ImageUsageFlagBits::eTransferSrc;

	// First pass: required + extra usage
	for (auto& Fmt : SurfaceFormats) {
		if (IsFormatUsageSupported(Fmt.format, RequiredUsage | ExtraUsage)) {
			SurfaceFormat = Fmt.format;
			SurfaceColorSpace = Fmt.colorSpace;
			break;
		}
	}

	// Second pass: fallback – required usage only
	if (SurfaceFormat == vk::Format::eUndefined) {
		for (auto& Fmt : SurfaceFormats) {
			if (IsFormatUsageSupported(Fmt.format, RequiredUsage)) {
				SurfaceFormat = Fmt.format;
				SurfaceColorSpace = Fmt.colorSpace;
				break;
			}
		}
	}

	if (SurfaceFormat == vk::Format::eUndefined) {
		throw std::runtime_error("No suitable swapchain format found");
	}

	// Choose present mode (prefer mailbox, then FIFO)
	vk::PresentModeKHR PresentMode = vk::PresentModeKHR::eFifo; // Fallback default
	for (auto& AvailablePresentMode : PresentModes)
	{
		if(AvailablePresentMode == vk::PresentModeKHR::eMailbox)
		{
			PresentMode = AvailablePresentMode;
			break;
		}
	}

	// Determine swap extent (use surface capabilities)
	SwapchainExtent = SurfaceCaps.currentExtent;
	if (SwapchainExtent.width == UINT32_MAX)
	{
		// Surface size is not defined, fallback
		SwapchainExtent.width = std::clamp(1920u, SurfaceCaps.minImageExtent.width, SurfaceCaps.maxImageExtent.width);
		SwapchainExtent.height = std::clamp(1080u, SurfaceCaps.minImageExtent.height, SurfaceCaps.maxImageExtent.height);
	}

	// Image count: tripple buffering (min + 1, at least 3)
	uint32_t ImageCount = std::max(SurfaceCaps.minImageCount + 1, 3u);

	if (SurfaceCaps.maxImageCount > 0)
	{
		ImageCount = std::min(ImageCount, SurfaceCaps.maxImageCount); // Clamp to max if there is a limit
	}

	vk::SwapchainCreateInfoKHR SwapchainCreateInfo;

	SwapchainCreateInfo.setSurface(*Surface)
		.setMinImageCount(ImageCount)
		.setImageFormat(SurfaceFormat)
		.setImageColorSpace(SurfaceColorSpace)
		.setImageExtent(SwapchainExtent)
		.setImageArrayLayers(1)											
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)	// Usage
		.setImageSharingMode(vk::SharingMode::eExclusive)												// Pre-transform
		.setPreTransform(SurfaceCaps.currentTransform)
		.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
		.setPresentMode(PresentMode)																	// Sharing mode
		.setClipped(VK_TRUE);																			// Clipped

	Swapchain = vk::raii::SwapchainKHR(Device.createSwapchainKHR(SwapchainCreateInfo, nullptr));
	SwapchainImages = Swapchain.getImages();
	SwapchainImageFormat = SurfaceFormat;

	// Create image views for swapchain images
	SwapchainImageViews.clear();
	SwapchainImageViews.reserve(SwapchainImages.size());

	for (auto& Image : SwapchainImages)
	{
		vk::ImageViewCreateInfo ViewCreateInfo
		(
			{},						// Flags
			Image,					// Image
			vk::ImageViewType::e2D,
			SwapchainImageFormat,
			vk::ComponentMapping(),	// Component mapping (default)
			{						// Subresource range
				vk::ImageAspectFlagBits::eColor,
				0, 1,				// Mip levels
				0, 1				// Array layers
			}
		);
		SwapchainImageViews.emplace_back(Device, ViewCreateInfo); // Create and store image view
	}
}

void Renderer::CreateSyncObjects()
{
	ImageAvailableSemaphores.clear();
	RenderFinishedSemaphores.clear();
	InFlightFences.clear();
	
	// Per‑frame semaphores for acquisition
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		ImageAvailableSemaphores.emplace_back(Device, vk::SemaphoreCreateInfo());
	}

	// Per‑image semaphores for rendering finished (size = swapchain image count)
	for (size_t i = 0; i < SwapchainImages.size(); i++)
	{
		RenderFinishedSemaphores.emplace_back(Device, vk::SemaphoreCreateInfo());
	}

	// Fences stay per‑frame
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		InFlightFences.emplace_back(Device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));	// Start signaled so we can wait on it immediately
	}
}

void Renderer::SetupRenderPasses()
{
	RendergraphPtr->AddResource(
		"Main_Color",
		vk::Format::eB8G8R8A8Unorm,					// Format – swapchain compatible
		SwapchainExtent,							// Extent – screen size
		vk::ImageUsageFlagBits::eColorAttachment |	// Usage – render target
		vk::ImageUsageFlagBits::eTransferSrc |		// Usage – also allow transfer to swapchain for presentation
		vk::ImageUsageFlagBits::eSampled,			// Usage - allow sampling for post-process or UI
		vk::ImageAspectFlagBits::eColor,			// Aspect – color only
		vk::ImageLayout::eUndefined,				// Initial layout – don't care (will be cleared)
		vk::ImageLayout::eTransferSrcOptimal		// Final layout – ready for copy to swapchain
	);

	RendergraphPtr->AddResource(
		"Main_Depth",
		vk::Format::eD32Sfloat,						// Format – 32‑bit float depth
		SwapchainExtent,							// Same extent as color
		vk::ImageUsageFlagBits::eDepthStencilAttachment, // Usage – depth testing
		vk::ImageAspectFlagBits::eDepth,			// Aspect – depth only
		vk::ImageLayout::eUndefined,				// Initial layout – undefined
		vk::ImageLayout::eDepthStencilAttachmentOptimal // Final layout – can stay as depth attachment (next frame will transition)
	);

	// TODO G buffer resources to add;

	auto GeomtryPass = RendergraphPtr->AddRenderPass<GeometryRenderPass>(
		"GeometryPass",
		CullingSystemPtr.get(), 
		"Main_Color", 
		"Main_Depth",
		std::move(DefaultPipeline),
		std::move(DefaultPipelineLayout),
		CameraUBO.get());

	//auto LightPass = RendergraphPtr->AddRenderPass<LightingPass>("LightPass", "Main_Color");
	//auto PostProcess = RendergraphPtr->AddRenderPass<PostProcessPass>("PostProcessPass", "Main_Color");
}

int Renderer::ScorePhysicalDevice(const vk::raii::PhysicalDevice& Dev, const vk::raii::SurfaceKHR& Surface)
{
	auto Props = Dev.getProperties();
	auto Features = Dev.getFeatures();

	// Memory size (device local)
	auto MemProps = Dev.getMemoryProperties();
	uint32_t VRAM = 0;

	for (auto& Heap : MemProps.memoryHeaps)
	{
		if(Heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal)
		{
			VRAM += Heap.size;
		}
	}

	int Score = 0;
	if (Props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
	{
		Score += 1000; // Discrete GPUs are preferred
	}
	else if (Props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
	{
		Score += 500; // Integrated GPUs are less powerful but more power efficient
	}

	Score += static_cast<int>(Props.limits.maxImageDimension2D / 1000); // Higher max texture size is better
	Score += static_cast<int>(VRAM / (1024 * 1024 * 100)); // More VRAM is better, per 100MB

	return Score;
}

bool Renderer::IsFormatUsageSupported(vk::Format Format, vk::ImageUsageFlags Usage)
{
	try {
		vk::ImageFormatProperties ImageFormatProps = PhysicalDevice.getImageFormatProperties(
			Format,
			vk::ImageType::e2D,
			vk::ImageTiling::eOptimal,
			Usage);
		return true;
	}
	catch (...) {
		return false;
	}
}
