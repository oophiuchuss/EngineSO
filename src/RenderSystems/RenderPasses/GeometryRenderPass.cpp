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
	std::string InGBufferAlbedoResourceName,
	std::string InGBufferNormalResourceName,
	std::string InGBufferMetalRoughResourceName,
	std::string InGBufferEmissiveResourceName,
	std::string InGBufferDepthResourceName,
	Shader* InGeometryShader,
	PipelineCache* InPipelineCache,
	CameraUniformBuffer* InCameraUBO,
	DescriptorHeap* InDescriptorHeap,
	GPUSceneBuffer* InGPUScene) :
	RenderPassBase(InName), 
	GBufferAlbedoResourceName(InGBufferAlbedoResourceName),
	GBufferNormalResourceName(InGBufferNormalResourceName),
	GBufferMetalRoughResourceName(InGBufferMetalRoughResourceName),
	GBufferEmissiveResourceName(InGBufferEmissiveResourceName),
	GBufferDepthResourceName(InGBufferDepthResourceName),
	GeometryShaderPtr(InGeometryShader),
	PipelineCachePtr(InPipelineCache),
	CameraUBOPtr(InCameraUBO),
	DescriptorHeapPtr(InDescriptorHeap),
	GPUScenePtr(InGPUScene)
{
	AddOutput(GBufferAlbedoResourceName, vk::ImageLayout::eColorAttachmentOptimal);
	AddOutput(GBufferNormalResourceName, vk::ImageLayout::eColorAttachmentOptimal);
	AddOutput(GBufferMetalRoughResourceName, vk::ImageLayout::eColorAttachmentOptimal);
	AddOutput(GBufferEmissiveResourceName, vk::ImageLayout::eColorAttachmentOptimal);
	AddOutput(GBufferDepthResourceName, vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

void GeometryRenderPass::BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
	Resource* Albedo = Graph.GetResource(GBufferAlbedoResourceName);

	const Rendergraph& RGraph = Graph;

	// Helper to build a clear-on-load color attachment
	auto MakeColorAttachment = [&Graph](const std::string& ResName, std::array<float, 4> ClearColor)
		{
			vk::RenderingAttachmentInfoKHR Attachment;
			Attachment.setImageView(Graph.GetResourceView(ResName))
				.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setLoadOp(vk::AttachmentLoadOp::eClear)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setClearValue(vk::ClearColorValue(ClearColor));
			return Attachment;
		};

	// Set up color attachments
	std::array<vk::RenderingAttachmentInfoKHR, 4> ColorAttachments = {
		MakeColorAttachment(GBufferAlbedoResourceName,     { 0.0f, 0.0f, 0.0f, 1.0f }),
		MakeColorAttachment(GBufferNormalResourceName,     { 0.0f, 0.0f, 0.0f, 0.0f }),
		MakeColorAttachment(GBufferMetalRoughResourceName, { 0.0f, 0.0f, 0.0f, 0.0f }),
		MakeColorAttachment(GBufferEmissiveResourceName,   { 0.0f, 0.0f, 0.0f, 0.0f }),
	};


	// Set up depth attachment
	vk::RenderingAttachmentInfoKHR DepthAttachment;
	DepthAttachment.setImageView(Graph.GetResourceView(GBufferDepthResourceName))
		.setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setClearValue(vk::ClearDepthStencilValue(1.0f, 0.0f));

	// Begin rendering with dynamic rendering 
	vk::RenderingInfoKHR RenderingInfo;
	RenderingInfo.setRenderArea(vk::Rect2D({ 0, 0 }, { Albedo->Extent.width, Albedo->Extent.height }))
		.setLayerCount(1)
		.setColorAttachmentCount(static_cast<uint32_t>(ColorAttachments.size()))
		.setPColorAttachments(ColorAttachments.data())
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

	Resource* Albedo = Graph.GetResource(GBufferAlbedoResourceName);
	Resource* Normal = Graph.GetResource(GBufferNormalResourceName);
	Resource* MetalRough = Graph.GetResource(GBufferMetalRoughResourceName);
	Resource* Emissive = Graph.GetResource(GBufferEmissiveResourceName);
	Resource* Depth = Graph.GetResource(GBufferDepthResourceName);

	// Build the base key from resource formats — same for every mesh this pass
	GraphicsPipelineKey Key;
	Key.ShaderPtr = GeometryShaderPtr;
	Key.ColorFormats = { Albedo->Format, Normal->Format, MetalRough->Format, Emissive->Format };
	Key.DepthFormat = Depth->Format;

	Key.DescriptorSetLayouts = {
		*CameraUBOPtr->GetDescriptorSetLayout(),
		*DescriptorHeapPtr->GetDescriptorSetLayout(),
		*GPUScenePtr->GetDescriptorSetLayout()
	};

	Key.PushConstantRange = vk::PushConstantRange(
		vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
		0,
		sizeof(PushConstantData));

	Key.bUseVertexInput = true;   // real vertex buffers, real meshes


	// Optain pipeline and layout from cache based on shader
	auto [Pipeline, PipelineLayout] = PipelineCachePtr->GetOrCreateGraphics(Key);

	// Bind pipeline
	Cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, Pipeline);

	// Bind descriptor sets (set 0 = camera UBO, set 1 = texture heap, set 2 = GPU scene buffer)
	std::array<vk::DescriptorSet, 3> DescriptorSets = {
		*CameraUBOPtr->GetDescriptorSet(CurrentFrameData.FrameIndex),
		*DescriptorHeapPtr->GetDescriptorSet(),
		*GPUScenePtr->GetDescriptorSet(CurrentFrameData.FrameIndex)
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
