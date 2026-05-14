module;

#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <optional>
#include <memory>

module Renderer;

import Rendergraph;
import CullingSystem;
import Entity;
import GeometryRenderPass;
import LightingPass;
import PostProcessPass;

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

	// Initialize rendergraph and CullingSystem

	RendergraphPtr = std::make_unique<Rendergraph>(Device, PhysicalDevice);
	CullingSystemPtr = std::make_unique<CullingSystem>(nullptr);   // no camera (we'll pass camera per frame)

	// Create command pool and buffers
	vk::CommandPoolCreateInfo PoolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, 
									   GraphicsQueueFamilyIndex);

	CommandPool = vk::raii::CommandPool(Device, PoolInfo);

	vk::CommandBufferAllocateInfo CmdAllocInfo(*CommandPool, vk::CommandBufferLevel::ePrimary, MAX_FRAMES_IN_FLIGHT);

	CommandBuffers = std::move(Device.allocateCommandBuffers(CmdAllocInfo));

	// Set up render passes and framebuffers in the rendergraph
	SetupRenderPasses();
}

Renderer::~Renderer()
{
}

void Renderer::RenderFrame(const std::vector<Entity*>& Entities)
{
	// Wait for previous frame to finish
	vk::Result FenceResult = Device.waitForFences({ *InFlightFences[CurrentFrame] }, VK_TRUE, UINT64_MAX);
	if (FenceResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to wait for in-flight fence");
	}
	
	Device.resetFences({ *InFlightFences[CurrentFrame] }); // Reset the fence for the current frame
	
	// Acquire next image from swapchain
	vk::Result AcquireNextImageResult;
	uint32_t ImageIndex;

	// Check for the feature test macro for the new API
#ifdef VULKAN_HPP_RAII_VERSION_1
	auto AcquireResult = Swapchain.acquireNextImage(UINT64_MAX, *ImageAvailableSemaphores[CurrentFrame]);
	AcquireNextImageResult = AcquireResult.result;
	ImageIndex = AcquireResult.value;
#else
	// Fallback for older versions returning std::pair
	std::tie(AcquireNextImageResult, ImageIndex) = Swapchain.acquireNextImage(UINT64_MAX, *ImageAvailableSemaphores[CurrentFrame]);
#endif

	if (AcquireNextImageResult == vk::Result::eErrorOutOfDateKHR) {
		RecreateSwapchain(0, 0);
		return;
	}
	else if (AcquireNextImageResult != vk::Result::eSuccess && AcquireNextImageResult != vk::Result::eSuboptimalKHR) {
		throw std::runtime_error("Failed to acquire swapchain image");
	}


	// Reset command buffer before usage
	auto& Cmd = CommandBuffers[CurrentFrame];
	Cmd.reset(); // Reset command buffer for recording
	
	vk::CommandBufferBeginInfo BeginInfo({});
	Cmd.begin(BeginInfo); // TODO: should this command buffer begin with no additional info?
	
	// Cull scene and update per-frame data
	CullingSystemPtr->CullScene(Entities);

	// Record rendering commands into command buffer using rendergraph
	RendergraphPtr->Execute(Cmd, GraphicsQueue);

	// Copy "Main_Color" to swapchain image (transition layouts accordingly)
	// TODO: implement blit/copy using rendergraph or manual barrier+copy.
	// For now, we'll just end command buffer and present (image will be black)

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
		.setPSignalSemaphores(&*RenderFinishedSemaphores[CurrentFrame]);


	GraphicsQueue.submit(SubmitInfo, *InFlightFences[CurrentFrame]);   // Submit to GPU queue

	// Present result
	vk::PresentInfoKHR PresentInfo;
	PresentInfo.setImageIndices(ImageIndex)
			   .setWaitSemaphoreCount(1)
			   .setPWaitSemaphores(&*RenderFinishedSemaphores[CurrentFrame])
			   .setSwapchainCount(1)
			   .setPSwapchains(&*Swapchain);

	vk::Result PresentResult = PresentQueue.presentKHR(PresentInfo);
	if (PresentResult == vk::Result::eErrorOutOfDateKHR || PresentResult == vk::Result::eSuboptimalKHR)
	{
		RecreateSwapchain(0, 0);
	}
	else if (PresentResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to present swapchain image");
	}

	CurrentFrame = (CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT; // Advance to next frame index
}

void Renderer::RecreateSwapchain(int Width, int Height)
{
	// Handle minimization of window

	if (Width == 0 || Height == 0)
	{
		return; // Skip recreation if window is minimized
	}

	Device.waitIdle(); // Wait for device to be idle before recreating swapchain

	// Clean up old swapchain resources
	SwapchainImageViews.clear();
	Swapchain.clear(); // vk::raii::SwapchainKHR will automatically clean up old swapchain and related resources when reassigned

	// Create new swapchain with updated dimensions
	CreateSwapchain();

	// Recreate framebuffers and render passes that depend on swapchain images 
	// TODO: rendergraph might need re-initializion
	
	SetupRenderPasses();
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

	vk::DeviceCreateInfo DeviceCreateInfo({}, QueueCreateInfos);
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

	// Choose surface format (prefer SRGB)
	vk::Format SurfaceFormat = vk::Format::eB8G8R8A8Unorm; // Fallback default
	vk::ColorSpaceKHR SurfaceColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
	for (auto& AvailableFormat : SurfaceFormats)
	{
		if(AvailableFormat.format == vk::Format::eB8G8R8A8Srgb && AvailableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
		{
			SurfaceFormat = AvailableFormat.format;
			SurfaceColorSpace = AvailableFormat.colorSpace;
			break;
		}
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
	uint32_t ImageCount = max(SurfaceCaps.minImageCount + 1, 3u);

	if (SurfaceCaps.maxImageCount > 0)
	{
		ImageCount = min(ImageCount, SurfaceCaps.maxImageCount); // Clamp to max if there is a limit
	}

	vk::SwapchainCreateInfoKHR SwapchainCreateInfo
	(
		{},																					// Flags
		*Surface,
		ImageCount,
		SurfaceFormat,
		SurfaceColorSpace,
		SwapchainExtent,
		1,																					// Image array layers
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,	// Usage
		vk::SharingMode::eExclusive,														// Sharing mode
		{},																					// Queue family indices if concurent (ignored for exclusive mode)
		SurfaceCaps.currentTransform,														// Pre-transform
		vk::CompositeAlphaFlagBitsKHR::eOpaque,												
		PresentMode,
		VK_TRUE,																			// Clipped
		nullptr																				// Old swapchain (for resizing)
	);

	Swapchain = vk::raii::SwapchainKHR(Device, SwapchainCreateInfo);
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
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		ImageAvailableSemaphores[i] = vk::raii::Semaphore(Device, vk::SemaphoreCreateInfo());
		RenderFinishedSemaphores[i] = vk::raii::Semaphore(Device, vk::SemaphoreCreateInfo());
		InFlightFences[i] = vk::raii::Fence(Device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)); // Start signaled so we can wait on it immediately
	}
}

void Renderer::SetupRenderPasses()
{
	RendergraphPtr->AddResource(
		"Main_Color",
		vk::Format::eB8G8R8A8Unorm,					// Format – swapchain compatible
		SwapchainExtent,							// Extent – screen size
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc, // Usage – render target + transfer to swapchain
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

	auto GeomtryPass = RendergraphPtr->AddRenderPass<GeometryRenderPass>("GeometryPass", CullingSystemPtr.get(), "Main_Color", "Main_Depth");
	auto LightPass = RendergraphPtr->AddRenderPass<LightingPass>("LightPass", "Main_Color");
	auto PostProcess = RendergraphPtr->AddRenderPass<PostProcessPass>("PostProcessPass", "Main_Color");
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
