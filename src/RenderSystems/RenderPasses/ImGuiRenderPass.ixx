module;

#include <string>
#include <vulkan/vulkan_raii.hpp>

export module ImGuiRenderPass;

import RenderPassBase;
import FrameData;

export class ImGuiRenderPass final : public RenderPassBase
{
public:
    ImGuiRenderPass(std::string InName, std::string InOutputResourceName);

protected:
    void BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;
    void ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;
    void EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;

private:
    std::string OutputResourceName;
};