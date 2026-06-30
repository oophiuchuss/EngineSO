module;

#include <vulkan/vulkan_raii.hpp>

module PostProcessStepPass;

import Rendergraph;

PostProcessStepPass::PostProcessStepPass(
    std::string InName,
    const std::string& InInputResourceName,
    const std::string& InOutputResourceName,
    Shader* InShader,
    PipelineCache* InPipelineCache,
    SingleTextureDescriptorSet* InInputDescSet):
    RenderPassBase(InName),
    InputResourceName(InInputResourceName),
    OutputResourceName(InOutputResourceName),
    ShaderPtr(InShader),
    PipelineCachePtr(InPipelineCache),
    InputDescSetPtr(InInputDescSet)
{
    AddInput(InputResourceName);
    AddOutput(OutputResourceName);
}

void PostProcessStepPass::BeginPass(
    vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame)
{
    Resource* Res = Graph.GetResource(OutputResourceName);
    vk::RenderingAttachmentInfoKHR Attach;
    Attach.setImageView(Graph.GetResourceView(OutputResourceName))
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfoKHR Info;
    Info.setRenderArea(vk::Rect2D({ 0,0 }, { Res->Extent.width, Res->Extent.height }))
        .setLayerCount(1)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&Attach);

    Cmd.beginRendering(Info);
}

void PostProcessStepPass::ExecuteMainLogic(
    vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame)
{
    Resource* Res = Graph.GetResource(OutputResourceName);

    PipelineKey Key;
    Key.ShaderPtr = ShaderPtr;
    Key.ColorFormats = { Res->Format };
    Key.DepthFormat = vk::Format::eUndefined;
    Key.DescriptorSetLayouts = { InputDescSetPtr->GetDescriptorSetLayout() };
    Key.bUseVertexInput = false;

    auto [Pipeline, PipelineLayout] = PipelineCachePtr->GetOrCreate(Key);

    Cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, Pipeline);
    Cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, PipelineLayout, 0,
        { InputDescSetPtr->GetDescriptorSet() }, {});

    Cmd.draw(3, 1, 0, 0);
}

void PostProcessStepPass::EndPass(
    vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame)
{
    Cmd.endRendering();
}