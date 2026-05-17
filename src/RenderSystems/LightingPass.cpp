module;

#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

module LightingPass;

import Rendergraph;

LightingPass::LightingPass(
	std::string InName, 
	std::string InGBufferColorResourceName) : 
	RenderPassBase(InName),
	GBufferColorResourceName(InGBufferColorResourceName)
{
	AddInput(GBufferColorResourceName);
	AddOutput(GBufferColorResourceName);
}

void LightingPass::AddLight(Light* LightToAdd)
{
	Lights.push_back(LightToAdd);
}

void LightingPass::RemoveLight(Light* LightToRemove)
{
	auto it = std::find(Lights.begin(), Lights.end(), LightToRemove);
	if (it != Lights.end())
	{
		Lights.erase(it);
	}
}

void LightingPass::BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph)
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

void LightingPass::ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph)
{
	// Get needed resources
	Resource* GBufferColor = Graph.GetResource(GBufferColorResourceName);

	// Set up descriptor sets for G-Buffer textures
	// With dynamic rendering we access G-Buffer textures directly as shader resources rather than as subpass input

	// Render full-screen quad with lighting shader
	
	// For each light
	for (const Light* CurLight : Lights)
	{
		// Set light props
		// Draw light volume
	}


}

void LightingPass::EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph)
{
	// End dynamic rendering
	Cmd.endRendering();		
}
