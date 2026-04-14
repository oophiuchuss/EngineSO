module;

#include <vulkan/vulkan_raii.hpp>
#include <vector>

module Renderer;

import Rendergraph;
import CullingSystem;
import Entity;
import GeometryRenderPass;
import LightingPass;
import PostProcessPass;

Renderer::Renderer(
	vk::raii::Device& Dev, 
	vk::Queue Queue, 
	vk::raii::PhysicalDevice& PhysicalDevice,
	Camera* Camera) :
	Device(Dev), 
	GraphicsQueue(Queue), 
	Rendergraph(Dev, PhysicalDevice),
	CullingSystem(Camera)
{
	// TODO
	// Create command pool

	// Create sync objects

	// Set up render passes

	SetupRenderPasses();
}

void Renderer::Render(const std::vector<Entity*>& Entities)
{
	// Wait for previous frame to finish
	//fence.wait(UINT64_MAX);
	//fence.reset();

	// Reset command buffer before usage
	CmdBuffer.reset();

	CullingSystem.CullScene(Entities);

	// Command buffer prep and Resource layout transitions
	// Set up command recording and transition resources to appropriate layouts
	CmdBuffer.begin({}); // TODO: should this command buffer begin with no additional info?

	Rendergraph.Execute(CmdBuffer, GraphicsQueue);

	// Command submission with sync
	// Submit buffer
	CmdBuffer.end();

	vk::PipelineStageFlags WaitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	vk::SubmitInfo SubmitInfo;
	SubmitInfo.setCommandBufferCount(1)		// Single command buffer
		.setPCommandBuffers(&*CmdBuffer)	// Command buffer to exec
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(&*ImageAvailableSemaphore)
		.setPWaitDstStageMask(WaitStages)
		.setSignalSemaphoreCount(1)
		.setPSignalSemaphores(&*RenderFinishedSemaphore);



	//GraphicsQueue.submit(1, SubmitInfo, *Fence);   // Submit to GPU queue
}

void Renderer::SetupRenderPasses()
{
	Rendergraph.AddResource(
		"Main_Color",
		vk::Format::eB8G8R8A8Unorm,              // Format – swapchain compatible
		vk::Extent2D(WindowWidth, WindowHeight), // Extent – screen size
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc, // Usage – render target + transfer to swapchain
		vk::ImageAspectFlagBits::eColor,         // Aspect – color only
		vk::ImageLayout::eUndefined,             // Initial layout – don't care (will be cleared)
		vk::ImageLayout::eTransferSrcOptimal     // Final layout – ready for copy to swapchain
	);

	Rendergraph.AddResource(
		"Main_Depth",
		vk::Format::eD32Sfloat,                  // Format – 32‑bit float depth
		vk::Extent2D(WindowWidth, WindowHeight), // Same extent as color
		vk::ImageUsageFlagBits::eDepthStencilAttachment, // Usage – depth testing
		vk::ImageAspectFlagBits::eDepth,         // Aspect – depth only
		vk::ImageLayout::eUndefined,             // Initial layout – undefined
		vk::ImageLayout::eDepthStencilAttachmentOptimal // Final layout – can stay as depth attachment (next frame will transition)
	);

	// TODO G buffer resources to add;

	auto GeomtryPass = Rendergraph.AddRenderPass<GeometryRenderPass>("GeometryPass", &CullingSystem, "Main_Color", "Main_Depth");
	auto LightPass = Rendergraph.AddRenderPass<LightingPass>("LightPass", "Main_Color");
	auto PostProcess = Rendergraph.AddRenderPass<PostProcessPass>("PostProcessPass", "Main_Color");
}
