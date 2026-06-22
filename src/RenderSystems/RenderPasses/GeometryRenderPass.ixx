module;

#include <string>
#include <vulkan/vulkan_raii.hpp>

export module GeometryRenderPass;

import RenderPassBase;
import CullingSystem;
import CameraUniform;
import DescriptorHeap;
import GPUSceneBuffer;
import FrameData;
import PipelineCache;
import Shader;

export class GeometryRenderPass : public RenderPassBase
{
public:
	explicit GeometryRenderPass(
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
		GPUSceneBuffer* InGPUScene);

	void SetRenderArea(vk::Extent2D InRenderArea) { RenderArea = InRenderArea; }

protected:
	virtual void BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;
	
	virtual void ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;
	
	virtual void EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;

private:
	std::string GBufferAlbedoResourceName;
	std::string GBufferNormalResourceName;
	std::string GBufferMetalRoughResourceName;
	std::string GBufferEmissiveResourceName;
	std::string GBufferDepthResourceName;

	Shader* GeometryShaderPtr = nullptr;
	PipelineCache* PipelineCachePtr = nullptr;
	CameraUniformBuffer* CameraUBOPtr = nullptr;
	DescriptorHeap* DescriptorHeapPtr = nullptr;
	GPUSceneBuffer* GPUScenePtr = nullptr;

	vk::Extent2D RenderArea = { 1280, 720 }; // Default render area, updated by Renderer
};