module;

#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

module PostProcessPass;

import Rendergraph;


PostProcessPass::PostProcessPass(
	std::string InName, 
	std::string InGBufferColorResourceName) : 
	RenderPassBase(InName), 
	GBufferColorResourceName(InGBufferColorResourceName)
{
	AddInput(GBufferColorResourceName);
	AddOutput(GBufferColorResourceName);
}

void PostProcessPass::AddPostProcessEffect(PostProcessEffect* EffectToAdd)
{
	Effects.push_back(EffectToAdd);
}

void PostProcessPass::RemovePostProcessEffect(PostProcessEffect* EffectToRemove)
{
	auto it = std::find(Effects.begin(), Effects.end(), EffectToRemove);
	if (it != Effects.end())
	{
		Effects.erase(it);
	}
}

void PostProcessPass::BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph)
{
	// Begin rendering with dynamic rendering 
	vk::RenderingInfoKHR RenderingInfo;

	// Get needed resources
	Resource* GBufferColor = Graph.GetResource(GBufferColorResourceName);

	// Set up color attachment
	vk::RenderingAttachmentInfoKHR ColorAttachment;
	ColorAttachment.setImageView(GBufferColor->View)
		.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)	// TODO: shouldn't be checked resource current layout?
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}));

	// Configure rendring info
	RenderingInfo.setRenderArea(vk::Rect2D({ 0, 0 }, { GBufferColor->Extent.width, GBufferColor->Extent.height }))
		.setLayerCount(1)
		.setColorAttachmentCount(1)
		.setPColorAttachments(&ColorAttachment);

	// Begin dynamic rendering
	Cmd.beginRendering(RenderingInfo);
}

void PostProcessPass::ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph)
{
	// With dynamic rendering, each effect can set up its own rendering state and access input textures directly as shader resources

	// Apply each post-process effect
	for (PostProcessEffect* CurEffect : Effects)
	{
		CurEffect->Apply(Cmd);
	}
}

void PostProcessPass::EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph)
{
	// End dynamic rendering
	Cmd.endRendering();
}
