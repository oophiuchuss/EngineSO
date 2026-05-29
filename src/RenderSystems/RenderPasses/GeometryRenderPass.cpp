module;

#include <string>
#include <vulkan/vulkan_raii.hpp>

module GeometryRenderPass;

import RenderPassBase;
import CullingSystem;
import Rendergraph;
import Entity;
import MeshComponent;
import Mesh;

GeometryRenderPass::GeometryRenderPass(
	std::string InName, 
	CullingSystem* InCulling, 
	std::string InGBufferColorResourceName, 
	std::string InGBufferDepthResourceName,
	vk::raii::Pipeline* InPipeline,
	vk::raii::PipelineLayout* InPipelineLayout,
	CameraUniformBuffer* InCameraUBO) :
	RenderPassBase(InName), 
	CullingSystemPtr(InCulling),
	GBufferColorResourceName(InGBufferColorResourceName),
	GBufferDepthResourceName(InGBufferDepthResourceName),
	Pipeline(InPipeline),
	PipelineLayout(InPipelineLayout),
	CameraUBOPtr(InCameraUBO)
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
	if (Pipeline == nullptr || PipelineLayout == nullptr)
	{
		throw std::runtime_error("Pipeline or pipeline layout not set for GeometryRenderPass");
	}
	
	// Set dynamic states 
	Cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, 
		static_cast<float>(RenderArea.width),
		static_cast<float>(RenderArea.height), 0.0f, 1.0f));

	Cmd.setScissor(0, vk::Rect2D({ 0, 0 }, RenderArea));

	// Bind pipeline
	Cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, **Pipeline);

	// Bind camera uniform buffer
	if (CameraUBOPtr)
	{
		CameraUBOPtr->Bind(*Cmd, **PipelineLayout);
	}

	// Get all visible entities
	const auto& AllEntities = CullingSystemPtr->GetAllVisibleEntities();

	for (const Entity* CurEntity : AllEntities)
	{
		// Get mesh and transform components
		MeshComponent* CurMeshComp = CurEntity->GetComponent<MeshComponent>();
		Mesh* CurMesh = CurMeshComp->GetMesh().get();

		// TODO: Add full implementation for drawing meshes, including setting model matrix and material properties. For now, just check if mesh exists and draw it.
		if (!CurMeshComp || !CurMesh)
		{
			continue;
		}

		VertexBuffer* VB = CurMesh->GetVertexBuffer();
		IndexBuffer* IB = CurMesh->GetIndexBuffer();

		// Check if only vertex buffer exists (e.g., for drawing non-indexed geometry)
		if (!VB)
		{
			continue;
		}

		VB->Bind(Cmd);

		if (IB)
		{
			IB->Bind(Cmd);
			Cmd.drawIndexed(IB->GetIndexCount(), 1, 0, 0, 0);
		}
		else
		{
			Cmd.draw(VB->GetVertexCount(), 1, 0, 0);
		}
	}
}

void GeometryRenderPass::EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph)
{
	// End dynamic rendering
	Cmd.endRendering();
}
