module;

#include <string>

#include <vulkan/vulkan_raii.hpp>

export module DirectionalShadowPass;

import RenderPassBase;
import FrameData;
import Shader;
import PipelineCache;
import DirectionalShadowDescriptorSet;
import GPUSceneBuffer;

export class DirectionalShadowPass : public RenderPassBase
{
public:
    DirectionalShadowPass(
        std::string InName,
        std::string InShadowMapResourceName,
        Shader* InShadowShader,
        PipelineCache* InPipelineCache,
        DirectionalShadowDescriptorSet* InShadowDescriptor,
        GPUSceneBuffer* InGPUScene);

protected:
    void BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;

    void ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;

    void EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;

private:
    std::string ShadowMapResourceName;

    Shader* ShadowShaderPtr = nullptr;
    PipelineCache* PipelineCachePtr = nullptr;

    DirectionalShadowDescriptorSet* ShadowDescriptorPtr = nullptr;

    GPUSceneBuffer* GPUScenePtr = nullptr;
};