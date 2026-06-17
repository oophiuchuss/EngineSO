module;

#include <string>
#include <vulkan/vulkan_raii.hpp>

module GeometryRenderPass;

import RenderPassBase;
import Rendergraph;
import Mesh;
import Shader;
import FrameData;
import PipelineCache;
import DescriptorHeap;
import GPUSceneBuffer;
import PushConstants;

GeometryRenderPass::GeometryRenderPass(
	std::string InName, 
	std::string InGBufferColorResourceName, 
	std::string InGBufferDepthResourceName,
	Shader* InGeometryShader,
	PipelineCache* InPipelineCache,
	CameraUniformBuffer* InCameraUBO,
	DescriptorHeap* InDescriptorHeap,
	GPUSceneBuffer* InGPUScene) :
	RenderPassBase(InName), 
	GBufferColorResourceName(InGBufferColorResourceName),
	GBufferDepthResourceName(InGBufferDepthResourceName),
	GeometryShaderPtr(InGeometryShader),
	PipelineCachePtr(InPipelineCache),
	CameraUBOPtr(InCameraUBO),
	DescriptorHeapPtr(InDescriptorHeap),
	GPUScenePtr(InGPUScene)
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
	if (!PipelineCachePtr)
	{
		throw std::runtime_error("Pipeline cache not set for GeometryRenderPass");
	}
	
	if (!GeometryShaderPtr)
	{
		throw std::runtime_error("Geometry shader not set for GeometryRenderPass");
	}

	if (!DescriptorHeapPtr)
	{
		throw std::runtime_error("Texture heap not set");
	}

	if (!GPUScenePtr)
	{
		throw std::runtime_error("GPU scene buffer not set");
	}

	// Set dynamic states 
	Cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, 
		static_cast<float>(RenderArea.width),
		static_cast<float>(RenderArea.height), 0.0f, 1.0f));

	Cmd.setScissor(0, vk::Rect2D({ 0, 0 }, RenderArea));

	// Build the base key from resource formats — same for every mesh this pass
	Resource* GBufferColor = Graph.GetResource(GBufferColorResourceName);
	Resource* GBufferDepth = Graph.GetResource(GBufferDepthResourceName);

	PipelineKey Key;
	Key.ShaderPtr = GeometryShaderPtr;
	Key.ColorFormats = { GBufferColor->Format };
	Key.DepthFormat = GBufferDepth->Format;

	// Optain pipeline and layout from cache based on shader
	auto [Pipeline, PipelineLayout] = PipelineCachePtr->GetOrCreate(Key);

	// Bind pipeline
	Cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, Pipeline);

	// Bind descriptor sets (set 0 = camera UBO, set 1 = texture heap, set 2 = GPU scene buffer)
	std::array<vk::DescriptorSet, 3> DescriptorSets = {
		CameraUBOPtr ? *CameraUBOPtr->GetDescriptorSet() : vk::DescriptorSet{},
		DescriptorHeapPtr ? *DescriptorHeapPtr->GetDescriptorSet() : vk::DescriptorSet{},
		GPUScenePtr ? *GPUScenePtr->GetDescriptorSet() : vk::DescriptorSet{}
	};

	Cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		PipelineLayout,
		0,                    // First set
		DescriptorSets,
		{});

	for (const RenderableMesh& Renderable : CurrentFrameData.Renderables)
	{
		if (!Renderable.GPUMesh)
		{
			continue;
		}

		// Build and push per-draw constants
		PushConstantData Push;
		Push.ObjectIndex = Renderable.ObjectIndex;

		Cmd.pushConstants<PushConstantData>(
			PipelineLayout, 
			vk::ShaderStageFlagBits::eVertex |
			vk::ShaderStageFlagBits::eFragment,
			0,
			Push);

		VertexBuffer* VB = Renderable.GPUMesh->GetVertexBuffer();
		IndexBuffer* IB = Renderable.GPUMesh->GetIndexBuffer();

		// Check if only vertex buffer exists (e.g., for drawing non-indexed geometry)
		if (VB)
		{
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
}

void GeometryRenderPass::EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
	// End dynamic rendering
	Cmd.endRendering();
}
