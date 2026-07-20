module;
#include <string>
#include <vulkan/vulkan_raii.hpp>
module ForwardTranslucencyPass;

import RenderPassBase;
import Rendergraph;
import Mesh;
import PushConstants;

ForwardTranslucencyPass::ForwardTranslucencyPass(
    std::string InName,
    std::string InColorResourceName,
    std::string InDepthResourceName,
    Shader* InShader,
    PipelineCache* InPipelineCache,
    CameraUniformBuffer* InCameraUBO,
    DescriptorHeap* InDescriptorHeap,
    GPUSceneBuffer* InGPUScene,
    LightBuffer* InLightBuffer) :
    RenderPassBase(InName),
    ColorResourceName(InColorResourceName),
    DepthResourceName(InDepthResourceName),
    ShaderPtr(InShader),
    PipelineCachePtr(InPipelineCache),
    CameraUBOPtr(InCameraUBO),
    DescriptorHeapPtr(InDescriptorHeap),
    GPUScenePtr(InGPUScene),
    LightBufferPtr(InLightBuffer)
{
    // Depth: test-only, no write - declared with the read-only depth layout
    AddInput(DepthResourceName, vk::ImageLayout::eDepthStencilReadOnlyOptimal);

    // Read-modify-write on Main_Color: depends on the other writer,
    // and re-declares the same layout as output since it's an attachment write, not a sample.
    AddInput(ColorResourceName, vk::ImageLayout::eColorAttachmentOptimal);
    AddOutput(ColorResourceName, vk::ImageLayout::eColorAttachmentOptimal);
}

void ForwardTranslucencyPass::BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
    Resource* ColorRes = Graph.GetResource(ColorResourceName);

    vk::RenderingAttachmentInfoKHR ColorAttachment;
    ColorAttachment.setImageView(Graph.GetResourceView(ColorResourceName))
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eLoad)         // preserve LightingPass's result
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfoKHR DepthAttachment;
    DepthAttachment.setImageView(Graph.GetResourceView(DepthResourceName))
        .setImageLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eLoad)         // preserve GeometryRenderPass's depth
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfoKHR RenderingInfo;
    RenderingInfo.setRenderArea(vk::Rect2D({ 0, 0 }, { ColorRes->Extent.width, ColorRes->Extent.height }))
        .setLayerCount(1)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&ColorAttachment)
        .setPDepthAttachment(&DepthAttachment);

    Cmd.beginRendering(RenderingInfo);
}

void ForwardTranslucencyPass::ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
    if (CurrentFrameData.TranslucentRenderables.empty())
    {
        return; // nothing to draw
    }

    Cmd.setViewport(0, vk::Viewport(0.0f, 0.0f,
        static_cast<float>(RenderArea.width), static_cast<float>(RenderArea.height), 0.0f, 1.0f));
    Cmd.setScissor(0, vk::Rect2D({ 0, 0 }, RenderArea));

    Resource* ColorRes = Graph.GetResource(ColorResourceName);
    Resource* DepthRes = Graph.GetResource(DepthResourceName);

    GraphicsPipelineKey Key;
    Key.ShaderPtr = ShaderPtr;
    Key.ColorFormats = { ColorRes->Format };
    Key.DepthFormat = DepthRes->Format;     // needed so depth test is enabled, even though write is off
    Key.DescriptorSetLayouts = {
        *CameraUBOPtr->GetDescriptorSetLayout(),
        *DescriptorHeapPtr->GetDescriptorSetLayout() ,
        *GPUScenePtr->GetDescriptorSetLayout() ,
        *LightBufferPtr->GetDescriptorSetLayout() 
    };
    Key.PushConstantRange = vk::PushConstantRange(
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstantData));
    Key.bUseVertexInput = true;
    Key.bEnableBlending = true;
    Key.bDepthWriteEnable = false;

    auto [Pipeline, PipelineLayout] = PipelineCachePtr->GetOrCreateGraphics(Key);

    Cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, Pipeline);

    std::array<vk::DescriptorSet, 4> DescriptorSets = {
        *CameraUBOPtr->GetDescriptorSet(CurrentFrameData.FrameIndex),
        *DescriptorHeapPtr->GetDescriptorSet(),
        *GPUScenePtr->GetDescriptorSet(CurrentFrameData.FrameIndex),
        *LightBufferPtr->GetDescriptorSet(CurrentFrameData.FrameIndex)
    };

    Cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, PipelineLayout, 0, DescriptorSets, {});

    // Already sorted back-to-front by Renderer::RenderFrame before this pass runs.
    for (const RenderableMesh& Renderable : CurrentFrameData.TranslucentRenderables)
    {
        if (!Renderable.GPUMesh) 
        {
            continue;
        }

        PushConstantData Push;
        Push.ObjectIndex = Renderable.ObjectIndex;
        Cmd.pushConstants<PushConstantData>(PipelineLayout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, Push);

        VertexBuffer* VB = Renderable.GPUMesh->GetVertexBuffer();
        IndexBuffer* IB = Renderable.GPUMesh->GetIndexBuffer();
        if (VB)
        {
            VB->Bind(Cmd);
            if (IB) 
            { 
                IB->Bind(Cmd);
                Cmd.drawIndexed(IB->GetIndexCount(), 1, 0, 0, 0);
            }
            else 
            { 
                Cmd.draw(VB->GetVertexCount(), 1, 0, 0); 
            }
        }
    }
}

void ForwardTranslucencyPass::EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
    Cmd.endRendering();
}