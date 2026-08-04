module;

#include <string>
#include <cstdint>

#include <vulkan/vulkan_raii.hpp>

export module TemporalAAPass;

import RenderPassBase;
import FrameData;
import Shader;
import PipelineCache;
import TemporalAADescriptorSet;
import FrameUniform;

export enum TemporalAAFlags : uint32_t
{
    TAA_HistoryValid = 1u << 0,
    TAA_AccumulationEnabled = 1u << 1
};

export struct TemporalAAPushConstants
{
    float HistoryWeight = 0.98f;
    float ResponsiveHistoryWeight = 0.9f;

    float DepthTolerance = 0.001f;

    float InverseWidth = 0.0f;
    float InverseHeight = 0.0f;

    uint32_t Flags = 0;
};

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
        FrameUniformBuffer* InFrameUniforms,
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

    FrameUniformBuffer* FrameUniformsPtr = nullptr;
    Shader* ShaderPtr = nullptr;
    PipelineCache* PipelineCachePtr = nullptr;
    TemporalAADescriptorSet* DescriptorSetPtr = nullptr;
};