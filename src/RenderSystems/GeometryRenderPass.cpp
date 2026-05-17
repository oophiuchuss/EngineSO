module;

#include <string>
#include <vulkan/vulkan_raii.hpp>

module GeometryRenderPass;

import RenderPassBase;
import CullingSystem;
import Rendergraph;
import Entity;
import MeshComponent;
import TransformComponent;

GeometryRenderPass::GeometryRenderPass(
	std::string InName, 
	CullingSystem* InCulling, 
	std::string InGBufferColorResourceName, 
	std::string InGBufferDepthResourceName) : 
	RenderPassBase(InName), 
	CullingSystemPtr(InCulling),
	GBufferColorResourceName(InGBufferColorResourceName),
	GBufferDepthResourceName(InGBufferDepthResourceName)
{
	AddOutput(GBufferColorResourceName);
	AddOutput(GBufferDepthResourceName);
}

void GeometryRenderPass::BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph)
{
	// Begin rendering with dynamic rendering 
	vk::RenderingInfoKHR RenderingInfo;

	// Get needed resources
	Resource* GBufferColor = Graph.GetResource(GBufferColorResourceName);
	Resource* GBufferDepth = Graph.GetResource(GBufferDepthResourceName);

	// Set up color attachment
	vk::RenderingAttachmentInfoKHR ColorAttachment;
	ColorAttachment.setImageView(GBufferColor->View)
		.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)	// TODO: shouldn't be checked resource current layout?
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}));
	
	// Set up depth attachment
	vk::RenderingAttachmentInfoKHR DepthAttachment;
	DepthAttachment.setImageView(GBufferDepth->View)
		.setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)	// TODO: shouldn't be checked resource current layout?
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setClearValue(vk::ClearDepthStencilValue(1.0f, 0.0f));

	// Configure rendring info
	RenderingInfo.setRenderArea(vk::Rect2D({ 0, 0 }, { GBufferColor->Extent.width, GBufferColor->Extent.height }))
		.setLayerCount(1)
		.setColorAttachmentCount(1)
		.setPColorAttachments(&ColorAttachment)
		.setPDepthAttachment(&DepthAttachment);

	// Begin dynamic rendering
	Cmd.beginRendering(RenderingInfo);
}

void GeometryRenderPass::ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph)
{
	// Get all visible entities
	const auto& AllEntities = CullingSystemPtr->GetAllVisibleEntities();

	for (const Entity* CurEntity : AllEntities)
	{
		// Get mesh and transform components
		MeshComponent* CurMeshComp = CurEntity->GetComponent<MeshComponent>();
		TransformComponent* CurTransfComp = CurEntity->GetComponent<TransformComponent>();

		if (CurMeshComp && CurTransfComp)
		{
			// TODO: Add implementation 
			// Bind pipeline for G-buffer rendering

			// Set model matrix

			//Draw mesh
		}
	}
}

void GeometryRenderPass::EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph)
{
	// End dynamic rendering
	Cmd.endRendering();
}
