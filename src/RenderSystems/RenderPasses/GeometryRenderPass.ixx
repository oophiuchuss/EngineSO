module;

#include <string>
#include <vulkan/vulkan_raii.hpp>

export module GeometryRenderPass;

import RenderPassBase;
import CullingSystem;
import CameraUniform;
import FrameData;

export class GeometryRenderPass : public RenderPassBase
{
public:
	explicit GeometryRenderPass(
		std::string InName,
		std::string InGBufferColorResourceName,
		std::string InGBufferDepthResourceName,
		vk::raii::Pipeline* InPipeline,
		vk::raii::PipelineLayout* InPipelineLayout,
		CameraUniformBuffer* InCameraUBO);

	void SetRenderArea(vk::Extent2D InRenderArea) { RenderArea = InRenderArea; }

protected:
	virtual void BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;
	
	virtual void ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;
	
	virtual void EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;

private:
	std::string GBufferColorResourceName; // TODO: potentially optimize access to pointers
	std::string GBufferDepthResourceName; // TODO: potentially optimize access to pointers

	// Pipeline
	vk::raii::Pipeline* Pipeline = nullptr;
	vk::raii::PipelineLayout* PipelineLayout = nullptr;
	CameraUniformBuffer* CameraUBOPtr = nullptr;

	vk::Extent2D RenderArea = { 1280, 720 }; // Default render area, updated by Renderer
};