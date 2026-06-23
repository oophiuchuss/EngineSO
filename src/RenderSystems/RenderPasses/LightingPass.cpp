module;

#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

module LightingPass;

import Rendergraph;
import FrameData;
import GBufferDescriptorSet;

LightingPass::LightingPass(
    std::string InName,
    std::string InOutputColorResourceName,
    std::string InGBufferAlbedoResourceName,
    std::string InGBufferNormalResourceName,
    std::string InGBufferMetalRoughResourceName,
    std::string InGBufferEmissiveResourceName,
    std::string InGBufferDepthResourceName,
    CameraUniformBuffer* InCameraUBO,
    LightBuffer* InLightBuffer
    GBufferDescriptorSet* InGBufferDescSet) :
    RenderPassBase(InName),
    OutputColorResourceName(InOutputColorResourceName),
    GBufferAlbedoResourceName(InGBufferAlbedoResourceName),
    GBufferNormalResourceName(InGBufferNormalResourceName),
    GBufferMetalRoughResourceName(InGBufferMetalRoughResourceName),
    GBufferEmissiveResourceName(InGBufferEmissiveResourceName),
    GBufferDepthResourceName(InGBufferDepthResourceName),
    CameraUBOPtr(InCameraUBO),
    LightBufferPtr(InLightBuffer),
    GBufferDescSetPtr(InGBufferDescSet)
{
    // Declare inputs (G‑buffer reads)
    AddInput(GBufferAlbedoResourceName);
    AddInput(GBufferNormalResourceName);
    AddInput(GBufferMetalRoughResourceName);
    AddInput(GBufferEmissiveResourceName);
    AddInput(GBufferDepthResourceName);

    // Declare output (lit result)
    AddOutput(OutputColorResourceName);
}

void LightingPass::BeginPass(
    vk::raii::CommandBuffer& Cmd,
    Rendergraph& Graph,
    FrameData& CurrentFrameData)
{
    Resource* OutputColor = Graph.GetResource(OutputColorResourceName);

    vk::RenderingAttachmentInfoKHR ColorAttachment;
    ColorAttachment.setImageView(OutputColor->View)
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue({ 0.0f, 0.0f, 0.0f, 1.0f }));

    vk::RenderingInfoKHR RenderingInfo;
    RenderingInfo.setRenderArea(
        vk::Rect2D({ 0, 0 }, { OutputColor->Extent.width, OutputColor->Extent.height }))
        .setLayerCount(1)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&ColorAttachment);

    Cmd.beginRendering(RenderingInfo);
}

void LightingPass::ExecuteMainLogic(
    vk::raii::CommandBuffer& Cmd,
    Rendergraph& Graph,
    FrameData& CurrentFrameData)
{
    // TODO: bind pipeline, descriptor sets, draw full‑screen triangle
}

void LightingPass::EndPass(
    vk::raii::CommandBuffer& Cmd,
    Rendergraph& Graph,
    FrameData& CurrentFrameData)
{
    Cmd.endRendering();
}
