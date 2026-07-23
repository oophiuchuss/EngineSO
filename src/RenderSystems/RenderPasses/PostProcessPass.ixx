module;

#include <string>
#include <vulkan/vulkan_raii.hpp>

export module PostProcessPass;

import RenderPassBase;
import FrameData;
import Shader;
import PipelineCache;
import SingleTextureDescriptorSet;
import PostProcessSettings;

export enum PostProcessFlags : uint32_t
{
    PP_ToneMapping = 1u << 0,
    PP_GammaCorrect = 1u << 1,
    PP_Dithering = 1u << 2,
    // reserved for later: PP_FXAA = 1u << 3, PP_DebugViewRaw = 1u << 4, ...
};

export struct PostProcessPushConstants
{
    float Exposure = 1.0f;
    uint32_t Flags = PP_ToneMapping | PP_GammaCorrect | PP_Dithering;
};

export class PostProcessPass : public RenderPassBase
{
public:
    PostProcessPass(
        std::string InName,
        const std::string& InInputResourceName,
        const std::string& InOutputResourceName,
        Shader* InShader,
        PipelineCache* InPipelineCache,
        SingleTextureDescriptorSet* InInputDescSet,
        const PostProcessSettings& InSettings);

    void BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame) override;
    void ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame) override;
    void EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& Frame) override;

    const PostProcessSettings& Settings;

private:
    std::string InputResourceName;
    std::string OutputResourceName;
    Shader* ShaderPtr;
    PipelineCache* PipelineCachePtr;
    SingleTextureDescriptorSet* InputDescSetPtr;
};