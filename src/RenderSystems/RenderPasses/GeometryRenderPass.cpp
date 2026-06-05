module;

#include <string>
#include <vulkan/vulkan_raii.hpp>

module GeometryRenderPass;

import RenderPassBase;
import Rendergraph;
import Mesh;
import FrameData;
import PipelineCache;
import PushConstants;

GeometryRenderPass::GeometryRenderPass(
	std::string InName, 
	std::string InGBufferColorResourceName, 
	std::string InGBufferDepthResourceName,
	PipelineCache* InPipelineCache,
	CameraUniformBuffer* InCameraUBO) :
	RenderPassBase(InName), 
	GBufferColorResourceName(InGBufferColorResourceName),
	GBufferDepthResourceName(InGBufferDepthResourceName),
	PipelineCachePtr(InPipelineCache),
	CameraUBOPtr(InCameraUBO)
{
	AddOutput(GBufferColorResourceName);
	AddOutput(GBufferDepthResourceName);
}

void GeometryRenderPass::BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
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

void GeometryRenderPass::ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
	if (PipelineCachePtr == nullptr)
	{
		throw std::runtime_error("Pipeline cache not set for GeometryRenderPass");
	}
	
	// Set dynamic states 
	Cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, 
		static_cast<float>(RenderArea.width),
		static_cast<float>(RenderArea.height), 0.0f, 1.0f));

	Cmd.setScissor(0, vk::Rect2D({ 0, 0 }, RenderArea));

	// Build the base key from resource formats — same for every mesh this pass
	Resource* GBufferColor = Graph.GetResource(GBufferColorResourceName);
	Resource* GBufferDepth = Graph.GetResource(GBufferDepthResourceName);

	PipelineKey BaseKey;
	BaseKey.ColorFormats = { GBufferColor->Format };
	BaseKey.DepthFormat = GBufferDepth->Format;

	for (const RenderableMesh& Renderable : CurrentFrameData.Renderables)
	{
		// TODO: Add full implementation for drawing meshes, including setting model matrix and material properties. For now, just check if mesh exists and draw it.
		if (!Renderable.GPUMesh || !Renderable.GPUShader)
		{
			continue;
		}
		
		// Per-mesh pipeline key differs only by shader

		PipelineKey MeshPipelineKey = BaseKey;
		MeshPipelineKey.ShaderPtr = Renderable.GPUShader;

		// Optain pipeline and layout from cache based on shader
		auto [Pipeline, PipelineLayout] = PipelineCachePtr->GetOrCreate(MeshPipelineKey);

		// Bind pipeline
		Cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, Pipeline);

		// Bind camera uniform buffer
		if (CameraUBOPtr)
		{
			CameraUBOPtr->Bind(*Cmd, PipelineLayout);
		}

		// Build and push per-draw constants
		PushConstantData Push;
		Push.ModelMatrix = Renderable.Transform;
		// TODO: Add material properties to push constants as well when implemented
		Push.Material.AlbedoColor = glm::vec4(1.0f);
		Push.Material.Metallic = 0.0f;
		Push.Material.Roughness = 0.5f;
		Push.Material.EmissiveStrength = 0.0f;

		Cmd.pushConstants<PushConstantData>(PipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, Push);

		VertexBuffer* VB = Renderable.GPUMesh->GetVertexBuffer();
		IndexBuffer* IB = Renderable.GPUMesh->GetIndexBuffer();

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

void GeometryRenderPass::EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
	// End dynamic rendering
	Cmd.endRendering();
}
