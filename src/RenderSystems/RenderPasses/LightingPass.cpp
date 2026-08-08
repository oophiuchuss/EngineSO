module;

#include <string>
#include <vector>
#include <array>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_raii.hpp>

module LightingPass;

import Rendergraph;
import FrameData;
import GBufferDescriptorSet;
import Shader;
import PipelineCache;
import DescriptorHeap;
import RenderDebugSettings;

LightingPass::LightingPass(
    std::string InName,
    std::string InOutputColorResourceName,
    std::string InGBufferAlbedoResourceName,
    std::string InGBufferNormalResourceName,
    std::string InGBufferMetalRoughResourceName,
    std::string InGBufferEmissiveResourceName,
    std::string InGBufferVelocityResourceName,
    std::string InGBufferDepthResourceName,
    std::string InDirectionalShadowResourceName,
    FrameUniformBuffer* InFrameUniforms,
    LightBuffer* InLightBuffer,
    GBufferDescriptorSet* InGBufferDescSet,
    DescriptorHeap* InDescriptorHeap,
    DirectionalShadowDescriptorSet* InDirectionalShadowDescriptor,
    Shader* InLightingShader,
    PipelineCache* InPipelineCache) :
    RenderPassBase(InName),
    OutputColorResourceName(std::move(InOutputColorResourceName)),
    GBufferAlbedoResourceName(std::move(InGBufferAlbedoResourceName)),
    GBufferNormalResourceName(std::move(InGBufferNormalResourceName)),
    GBufferMetalRoughResourceName(std::move(InGBufferMetalRoughResourceName)),
    GBufferEmissiveResourceName(std::move(InGBufferEmissiveResourceName)),
    GBufferVelocityResourceName(std::move(InGBufferVelocityResourceName)),
    GBufferDepthResourceName(std::move(InGBufferDepthResourceName)),
    DirectionalShadowResourceName(std::move(InDirectionalShadowResourceName)),
    FrameUniformsPtr(InFrameUniforms),
    LightBufferPtr(InLightBuffer),
    GBufferDescSetPtr(InGBufferDescSet),
    DescriptorHeapPtr(InDescriptorHeap),
    DirectionalShadowDescriptorPtr(InDirectionalShadowDescriptor),
    LightingShaderPtr(InLightingShader),
    PipelineCachePtr(InPipelineCache)
{
    // Declare inputs (G‑buffer reads)
    AddInput(GBufferAlbedoResourceName);
    AddInput(GBufferNormalResourceName);
    AddInput(GBufferMetalRoughResourceName);
    AddInput(GBufferEmissiveResourceName);
	AddInput(GBufferVelocityResourceName);
    AddInput(GBufferDepthResourceName);
    AddInput(DirectionalShadowResourceName);

    // Declare output (lit result)
    AddOutput(OutputColorResourceName, vk::ImageLayout::eColorAttachmentOptimal);
}

void LightingPass::BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
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

void LightingPass::ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
    if (!PipelineCachePtr)
    {
        throw std::runtime_error("LightingPass: pipeline cache not set");
    }

    if (!LightingShaderPtr)
    {
        throw std::runtime_error("LightingPass: shader not set");
    }

    if (!FrameUniformsPtr)
    {
        throw std::runtime_error("LightingPass: frame uniforms not set");
    }

    if (!GBufferDescSetPtr)
    {
        throw std::runtime_error("LightingPass: GBuffer descriptors not set");
    }

    if (!LightBufferPtr)
    {
        throw std::runtime_error("LightingPass: light buffer not set");
    }

    if (!DescriptorHeapPtr)
    {
        throw std::runtime_error("LightingPass: descriptor heap not set");
    }
    
    if (!DirectionalShadowDescriptorPtr)
    {
        throw std::runtime_error("LightingPass: directional shadow descriptor not set");
    }

    Resource* OutputColor = Graph.GetResource(OutputColorResourceName);

    GraphicsPipelineKey Key;

    Key.ShaderPtr = LightingShaderPtr;
    Key.ColorFormats = { OutputColor->Format };
    Key.DepthFormat = vk::Format::eUndefined;

    Key.DescriptorSetLayouts = {
        *FrameUniformsPtr->GetDescriptorSetLayout(),
        *GBufferDescSetPtr->GetDescriptorSetLayout(),
        *LightBufferPtr->GetDescriptorSetLayout(),
        *DescriptorHeapPtr->GetDescriptorSetLayout(),
        *DirectionalShadowDescriptorPtr->GetDescriptorSetLayout()
    };

    Key.PushConstantRange = vk::PushConstantRange(
        vk::ShaderStageFlagBits::eFragment,
        0,
        sizeof(LightingPushConstants));

    Key.VertexInput = VertexInputMode::None;

    auto [Pipeline, PipelineLayout] = PipelineCachePtr->GetOrCreateGraphics(Key);

    Cmd.bindPipeline(vk::PipelineBindPoint::eGraphics,Pipeline);

    const vk::Extent2D RenderExtent = OutputColor->Extent;

    Cmd.setViewport(0, vk::Viewport(0.0f, 0.0f,
            static_cast<float>(RenderExtent.width),
            static_cast<float>(RenderExtent.height),
            0.0f, 1.0f));

    Cmd.setScissor(0, vk::Rect2D({ 0, 0 }, RenderExtent));

    std::array<vk::DescriptorSet, 5> DescriptorSets = {
        *FrameUniformsPtr->GetDescriptorSet(CurrentFrameData.FrameIndex),
        *GBufferDescSetPtr->GetDescriptorSet(),
        *LightBufferPtr->GetDescriptorSet(CurrentFrameData.FrameIndex),
        *DescriptorHeapPtr->GetDescriptorSet(),
        *DirectionalShadowDescriptorPtr->GetDescriptorSet(CurrentFrameData.FrameIndex)
    };

    Cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        PipelineLayout,
        0,
        DescriptorSets,
        {});

    LightingPushConstants PushConstants;

    PushConstants.DebugView = static_cast<uint32_t>(CurrentFrameData.DebugSettings.View);

    PushConstants.MotionVectorScale = CurrentFrameData.DebugSettings.MotionVectorScale;

    Cmd.pushConstants<LightingPushConstants>(
        PipelineLayout,
        vk::ShaderStageFlagBits::eFragment,
        0,
        PushConstants);

    Cmd.draw(3, 1, 0, 0);
}

void LightingPass::EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
    Cmd.endRendering();
}
