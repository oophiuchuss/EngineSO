module;

#include <array>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_raii.hpp>

module TemporalAAPass;

import Rendergraph;

TemporalAAPass::TemporalAAPass(
    std::string InName,
    std::string InCurrentColorResourceName,
    std::string InCurrentDepthResourceName,
    std::string InVelocityResourceName,
    std::string InHistoryColorReadResourceName,
    std::string InHistoryDepthReadResourceName,
    std::string InHistoryColorWriteResourceName,
    std::string InHistoryDepthWriteResourceName,
    Shader* InShader,
    PipelineCache* InPipelineCache,
    TemporalAADescriptorSet* InDescriptorSet):
    RenderPassBase(std::move(InName)),
    CurrentColorResourceName(std::move(InCurrentColorResourceName)),
    CurrentDepthResourceName(std::move(InCurrentDepthResourceName)),
    VelocityResourceName(std::move(InVelocityResourceName)),
    HistoryColorReadResourceName(std::move(InHistoryColorReadResourceName)),
    HistoryDepthReadResourceName(std::move(InHistoryDepthReadResourceName)),
    HistoryColorWriteResourceName(std::move(InHistoryColorWriteResourceName)),
    HistoryDepthWriteResourceName(std::move(InHistoryDepthWriteResourceName)),
    ShaderPtr(InShader),
    PipelineCachePtr(InPipelineCache),
    DescriptorSetPtr(InDescriptorSet)
{
    AddInput(CurrentColorResourceName);
    AddInput(CurrentDepthResourceName);
    AddInput(VelocityResourceName);
    AddInput(HistoryColorReadResourceName);
    AddInput(HistoryDepthReadResourceName);

    AddOutput(HistoryColorWriteResourceName, vk::ImageLayout::eColorAttachmentOptimal);

    AddOutput(HistoryDepthWriteResourceName, vk::ImageLayout::eColorAttachmentOptimal);
}

void TemporalAAPass::BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame)
{
    Resource* ColorOutput = Graph.GetResource(HistoryColorWriteResourceName);

    std::array<vk::RenderingAttachmentInfo, 2> Attachments;

    Attachments[0]
        .setImageView(Graph.GetResourceView(HistoryColorWriteResourceName))
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    Attachments[1]
        .setImageView(Graph.GetResourceView(HistoryDepthWriteResourceName))
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfo RenderingInfo;
    RenderingInfo
        .setRenderArea(vk::Rect2D(
                { 0, 0 },
                { ColorOutput->Extent.width, ColorOutput->Extent.height }))
        .setLayerCount(1)
        .setColorAttachments(Attachments);

    Cmd.beginRendering(RenderingInfo);
}

void TemporalAAPass::ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame)
{
    if (!ShaderPtr || !PipelineCachePtr || !DescriptorSetPtr)
    {
        throw std::runtime_error("TemporalAAPass dependencies are not initialized");
    }

    Resource* ColorOutput = Graph.GetResource(HistoryColorWriteResourceName);

    Resource* DepthOutput = Graph.GetResource(HistoryDepthWriteResourceName);

    GraphicsPipelineKey Key;
    Key.ShaderPtr = ShaderPtr;
    Key.ColorFormats =
    {
        ColorOutput->Format,
        DepthOutput->Format
    };
    Key.DepthFormat = vk::Format::eUndefined;
    Key.DescriptorSetLayouts =
    {
        *DescriptorSetPtr->GetDescriptorSetLayout()
    };
    Key.bUseVertexInput = false;
    Key.bDepthWriteEnable = false;

    auto [Pipeline, PipelineLayout] = PipelineCachePtr->GetOrCreateGraphics(Key);

    Cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, Pipeline);

    Cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        PipelineLayout,
        0,
        { *DescriptorSetPtr->GetDescriptorSet(Frame.FrameIndex) },
        {});

    Cmd.draw(3, 1, 0, 0);
}

void TemporalAAPass::EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame)
{
    Cmd.endRendering();
}