module;

#include <string>
#include <vulkan/vulkan_raii.hpp>

export module TemporalAAPass;

import RenderPassBase;
import FrameData;
import Shader;
import PipelineCache;
import TemporalAADescriptorSet;

export class TemporalAAPass : public RenderPassBase
{
public:
    TemporalAAPass(
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
        TemporalAADescriptorSet* InDescriptorSet);

protected:
    void BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame) override;

    void ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame) override;

    void EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame) override;

private:
    std::string CurrentColorResourceName;
    std::string CurrentDepthResourceName;
    std::string VelocityResourceName;

    std::string HistoryColorReadResourceName;
    std::string HistoryDepthReadResourceName;
    std::string HistoryColorWriteResourceName;
    std::string HistoryDepthWriteResourceName;

    Shader* ShaderPtr = nullptr;
    PipelineCache* PipelineCachePtr = nullptr;
    TemporalAADescriptorSet* DescriptorSetPtr = nullptr;
};