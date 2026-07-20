module;

#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

module LightingPass;

import Rendergraph;
import FrameData;
import GBufferDescriptorSet;
import Shader;
import PipelineCache;

LightingPass::LightingPass(
    std::string InName,
    std::string InOutputColorResourceName,
    std::string InGBufferAlbedoResourceName,
    std::string InGBufferNormalResourceName,
    std::string InGBufferMetalRoughResourceName,
    std::string InGBufferEmissiveResourceName,
    std::string InGBufferDepthResourceName,
    CameraUniformBuffer* InCameraUBO,
    LightBuffer* InLightBuffer,
    GBufferDescriptorSet* InGBufferDescSet,
    Shader* InLightingShader,
    PipelineCache* InPipelineCache) :
    RenderPassBase(InName),
    OutputColorResourceName(InOutputColorResourceName),
    GBufferAlbedoResourceName(InGBufferAlbedoResourceName),
    GBufferNormalResourceName(InGBufferNormalResourceName),
    GBufferMetalRoughResourceName(InGBufferMetalRoughResourceName),
    GBufferEmissiveResourceName(InGBufferEmissiveResourceName),
    GBufferDepthResourceName(InGBufferDepthResourceName),
    CameraUBOPtr(InCameraUBO),
    LightBufferPtr(InLightBuffer),
    GBufferDescSetPtr(InGBufferDescSet),
    LightingShaderPtr(InLightingShader),
    PipelineCachePtr(InPipelineCache)
{
    // Declare inputs (G‑buffer reads)
    AddInput(GBufferAlbedoResourceName);
    AddInput(GBufferNormalResourceName);
    AddInput(GBufferMetalRoughResourceName);
    AddInput(GBufferEmissiveResourceName);
    AddInput(GBufferDepthResourceName);

    // Declare output (lit result)
    AddOutput(OutputColorResourceName, vk::ImageLayout::eColorAttachmentOptimal);
}

void LightingPass::BeginPass(
    vk::raii::CommandBuffer& Cmd,
    Rendergraph& Graph,
    FrameData& CurrentFrameData)
{
    Resource* OutputColor = Graph.GetResource(OutputColorResourceName);

    vk::RenderingAttachmentInfoKHR ColorAttachment;
    ColorAttachment.setImageView(Graph.GetResourceView(OutputColorResourceName))
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
    if (!PipelineCachePtr)
    {
        throw std::runtime_error("LightingPass: pipeline cache not set");
    }
    
    if (!LightingShaderPtr)
    {
        throw std::runtime_error("LightingPass: shader not set");
    }

    Resource* OutputColor = Graph.GetResource(OutputColorResourceName);

    // Build pipeline key for this pass
    GraphicsPipelineKey Key;
    Key.ShaderPtr = LightingShaderPtr;
    Key.ColorFormats = { OutputColor->Format };
    Key.DepthFormat = vk::Format::eUndefined;          // no depth attachment
    Key.DescriptorSetLayouts = {
        *CameraUBOPtr->GetDescriptorSetLayout(),
        *GBufferDescSetPtr->GetDescriptorSetLayout(),
        *LightBufferPtr->GetDescriptorSetLayout()
    };
    Key.PushConstantRange = vk::PushConstantRange{};   // no push constants in lighting pass
    Key.bUseVertexInput = false;                       // full‑screen triangle, no vertex buffer

    auto [Pipeline, PipelineLayout] = PipelineCachePtr->GetOrCreateGraphics(Key);

    Cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, Pipeline);

    // Bind all descriptor sets at once
    std::array<vk::DescriptorSet, 3> DescriptorSets = {
        *CameraUBOPtr->GetDescriptorSet(CurrentFrameData.FrameIndex),
        *GBufferDescSetPtr->GetDescriptorSet(),
        *LightBufferPtr->GetDescriptorSet(CurrentFrameData.FrameIndex)
    };

    Cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        PipelineLayout,
        0,
        DescriptorSets,
        {});

    // Draw the full‑screen triangle (3 vertices, no vertex buffer)
    Cmd.draw(3, 1, 0, 0);
}

void LightingPass::EndPass(
    vk::raii::CommandBuffer& Cmd,
    Rendergraph& Graph,
    FrameData& CurrentFrameData)
{
    Cmd.endRendering();
}
