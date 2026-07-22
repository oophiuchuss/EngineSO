module;

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_raii.hpp>

module ImGuiRenderPass;

import Rendergraph;
import FrameData;

ImGuiRenderPass::ImGuiRenderPass(
    std::string InName,
    std::string InOutputResourceName):
    RenderPassBase(std::move(InName)),
    OutputResourceName(std::move(InOutputResourceName))
{
    AddInput(OutputResourceName, vk::ImageLayout::eColorAttachmentOptimal);
    AddOutput(OutputResourceName, vk::ImageLayout::eColorAttachmentOptimal);
}

void ImGuiRenderPass::BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
    Resource* Output = Graph.GetResource(OutputResourceName);
    if (!Output)
    {
        throw std::runtime_error("ImGuiRenderPass: output resource not found");
    }

    vk::RenderingAttachmentInfo ColorAttachment;
    ColorAttachment.setImageView(Graph.GetResourceView(OutputResourceName))
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eLoad)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfo RenderingInfo;
    RenderingInfo.setRenderArea(vk::Rect2D({ 0, 0 }, Output->Extent))
        .setLayerCount(1)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&ColorAttachment);

    Cmd.beginRendering(RenderingInfo);
}

void ImGuiRenderPass::ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
    if (!CurrentFrameData.ImGuiDrawData)
    {
        return;
    }

    ImGui_ImplVulkan_RenderDrawData(CurrentFrameData.ImGuiDrawData, static_cast<VkCommandBuffer>(*Cmd));
}

void ImGuiRenderPass::EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
{
    Cmd.endRendering();
}
