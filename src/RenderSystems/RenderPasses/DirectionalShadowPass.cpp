module;

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

#include <vulkan/vulkan_raii.hpp>

module DirectionalShadowPass;

import Rendergraph;
import Mesh;
import PushConstants;

DirectionalShadowPass::DirectionalShadowPass(
    std::string InName,
    std::string InShadowMapResourceName,
    Shader* InShadowShader,
    PipelineCache* InPipelineCache,
    DirectionalShadowDescriptorSet* InShadowDescriptor,
    GPUSceneBuffer* InGPUScene) :
    RenderPassBase(std::move(InName)),
    ShadowMapResourceName(std::move(InShadowMapResourceName)),
    ShadowShaderPtr(InShadowShader),
    PipelineCachePtr(InPipelineCache),
    ShadowDescriptorPtr(InShadowDescriptor),
    GPUScenePtr(InGPUScene)
{
    AddOutput(ShadowMapResourceName, vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

void DirectionalShadowPass::BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
    Resource* ShadowMap = Graph.GetResource(ShadowMapResourceName);

    if (!ShadowMap)
    {
        throw std::runtime_error("DirectionalShadowPass: shadow-map resource not found");
    }

    vk::RenderingAttachmentInfoKHR DepthAttachment;

    DepthAttachment.setImageView(Graph.GetResourceView(ShadowMapResourceName))
        .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

    vk::RenderingInfoKHR RenderingInfo;

    RenderingInfo.setRenderArea(vk::Rect2D({ 0, 0 }, ShadowMap->Extent))
        .setLayerCount(1)
        .setColorAttachmentCount(0)
        .setPColorAttachments(nullptr)
        .setPDepthAttachment(&DepthAttachment);

    Cmd.beginRendering(RenderingInfo);
}

void DirectionalShadowPass::ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
    if (!ShadowShaderPtr)
    {
        throw std::runtime_error("DirectionalShadowPass: shader not set");
    }

    if (!PipelineCachePtr)
    {
        throw std::runtime_error("DirectionalShadowPass: pipeline cache not set");
    }

    if (!ShadowDescriptorPtr)
    {
        throw std::runtime_error("DirectionalShadowPass: descriptor set not set");
    }

    if (!GPUScenePtr)
    {
        throw std::runtime_error("DirectionalShadowPass: GPU scene not set");
    }

    Resource* ShadowMap = Graph.GetResource(ShadowMapResourceName);

    if (!ShadowMap)
    {
        throw std::runtime_error("DirectionalShadowPass: shadow-map resource not found");
    }

    GraphicsPipelineKey Key;

    Key.ShaderPtr = ShadowShaderPtr;
    Key.ColorFormats = {};
    Key.DepthFormat = ShadowMap->Format;

    Key.DescriptorSetLayouts =
    {
        *ShadowDescriptorPtr->GetDescriptorSetLayout(),
        *GPUScenePtr->GetDescriptorSetLayout()
    };

    Key.PushConstantRange = vk::PushConstantRange(
        vk::ShaderStageFlagBits::eVertex,
        0,
        sizeof(PushConstantData));

    Key.VertexInput = VertexInputMode::PositionOnly;
    Key.bEnableBlending = false;
    Key.bDepthWriteEnable = true;
    Key.bEnableDepthBias = true;

    const PipelineHandles Handles = PipelineCachePtr->GetOrCreateGraphics(Key);

    Cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, Handles.Pipeline);

    Cmd.setViewport(0 ,vk::Viewport(0.0f, 0.0f,
            static_cast<float>(ShadowMap->Extent.width),
            static_cast<float>(ShadowMap->Extent.height), 0.0f, 1.0f));

    Cmd.setScissor(0, vk::Rect2D({ 0, 0 }, ShadowMap->Extent));

    Cmd.setDepthBias(
        CurrentFrameData.ShadowSettings.RasterConstantBias,
        0.0f,
        CurrentFrameData.ShadowSettings.RasterSlopeBias);

    const std::array<vk::DescriptorSet, 2> DescriptorSets =
    {
        *ShadowDescriptorPtr->GetDescriptorSet(CurrentFrameData.FrameIndex),
        *GPUScenePtr->GetDescriptorSet(CurrentFrameData.FrameIndex)
    };

    Cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        Handles.Layout,
        0,
        DescriptorSets,
        {});

    // The map was already cleared to fully lit. When no valid directional
    // shadow exists, no caster geometry needs to be submitted.
    if (CurrentFrameData.DirectionalShadow.Controls.x == 0)
    {
        return;
    }

    for (const RenderableMesh& Renderable : CurrentFrameData.ShadowCasters)
    {
        if (!Renderable.GPUMesh)
        {
            continue;
        }

        PushConstantData Push{};
        Push.ObjectIndex = Renderable.ObjectIndex;

        Cmd.pushConstants<PushConstantData>(Handles.Layout, vk::ShaderStageFlagBits::eVertex, 0, Push);

        VertexBuffer* VertexBufferPtr = Renderable.GPUMesh->GetVertexBuffer();

        IndexBuffer* IndexBufferPtr = Renderable.GPUMesh->GetIndexBuffer();

        if (!VertexBufferPtr)
        {
            continue;
        }

        VertexBufferPtr->Bind(Cmd);

        if (IndexBufferPtr)
        {
            IndexBufferPtr->Bind(Cmd);

            Cmd.drawIndexed(IndexBufferPtr->GetIndexCount(), 1, 0, 0, 0);
        }
        else
        {
            Cmd.draw(VertexBufferPtr->GetVertexCount(), 1, 0, 0);
        }
    }
}

void DirectionalShadowPass::EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
    Cmd.endRendering();
}