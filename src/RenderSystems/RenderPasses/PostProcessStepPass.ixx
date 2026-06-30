module;

#include <string>
#include <vulkan/vulkan_raii.hpp>

export module PostProcessStepPass;

import RenderPassBase;
import FrameData;
import Shader;
import PipelineCache;
import SingleTextureDescriptorSet;

export class PostProcessStepPass : public RenderPassBase
{
public:
    PostProcessStepPass(
        std::string InName,
        const std::string& InInputResourceName,
        const std::string& InOutputResourceName,
        Shader* InShader,
        PipelineCache* InPipelineCache,
        SingleTextureDescriptorSet* InInputDescSet);

    void BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame) override;
    void ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame) override;
    void EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame) override;

private:
    std::string InputResourceName;
    std::string OutputResourceName;
    Shader* ShaderPtr;
    PipelineCache* PipelineCachePtr;
    SingleTextureDescriptorSet* InputDescSetPtr;
};
