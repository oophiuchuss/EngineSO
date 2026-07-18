module;
#include <vulkan/vulkan_raii.hpp>
module PostProcessPass;
import Rendergraph;

PostProcessPass::PostProcessPass(
    std::string InName,
    const std::string& InInputResourceName,
    const std::string& InOutputResourceName,
    Shader* InShader,
    PipelineCache* InPipelineCache,
    SingleTextureDescriptorSet* InInputDescSet) :
    RenderPassBase(InName),
    InputResourceName(InInputResourceName),
    OutputResourceName(InOutputResourceName),
    ShaderPtr(InShader),
    PipelineCachePtr(InPipelineCache),
    InputDescSetPtr(InInputDescSet)
{
    AddInput(InputResourceName);
    AddOutput(OutputResourceName, vk::ImageLayout::eColorAttachmentOptimal);
}

void PostProcessPass::BeginPass(
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

void PostProcessPass::ExecuteMainLogic(
    vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame)
{
    Resource* Res = Graph.GetResource(OutputResourceName);

    GraphicsPipelineKey Key;
    Key.ShaderPtr = ShaderPtr;
    Key.ColorFormats = { Res->Format };
    Key.DepthFormat = vk::Format::eUndefined;
    Key.DescriptorSetLayouts = { *InputDescSetPtr->GetDescriptorSetLayout() };
    Key.PushConstantRange = vk::PushConstantRange(
        vk::ShaderStageFlagBits::eFragment,
        0,
        sizeof(PostProcessPushConstants));
    Key.bUseVertexInput = false;

    auto [Pipeline, PipelineLayout] = PipelineCachePtr->GetOrCreateGraphics(Key);

    Cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, Pipeline);
    Cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, PipelineLayout, 0,
        { *InputDescSetPtr->GetDescriptorSet() }, {});
    Cmd.pushConstants<PostProcessPushConstants>(
        PipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, PushConstants);

    Cmd.draw(3, 1, 0, 0);
}

void PostProcessPass::EndPass(
    vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame)
{
    Cmd.endRendering();
}