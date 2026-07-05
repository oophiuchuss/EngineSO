module;
#include <string>
#include <vulkan/vulkan_raii.hpp>
export module ForwardTranslucencyPass;

import RenderPassBase;
import CameraUniform;
import DescriptorHeap;
import GPUSceneBuffer;
import LightBuffer;
import FrameData;
import PipelineCache;
import Shader;

export class ForwardTranslucencyPass : public RenderPassBase
{
public:
    explicit ForwardTranslucencyPass(
        std::string InName,
        std::string InColorResourceName,
        std::string InDepthResourceName,
        Shader* InShader,
        PipelineCache* InPipelineCache,
        CameraUniformBuffer* InCameraUBO,
        DescriptorHeap* InDescriptorHeap,
        GPUSceneBuffer* InGPUScene,
        LightBuffer* InLightBuffer);

    void SetRenderArea(vk::Extent2D InRenderArea) { RenderArea = InRenderArea; }

protected:
    void BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;
    void ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;
    void EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;

private:
    std::string ColorResourceName;
    std::string DepthResourceName;

    Shader* ShaderPtr = nullptr;
    PipelineCache* PipelineCachePtr = nullptr;
    CameraUniformBuffer* CameraUBOPtr = nullptr;
    DescriptorHeap* DescriptorHeapPtr = nullptr;
    GPUSceneBuffer* GPUScenePtr = nullptr;
    LightBuffer* LightBufferPtr = nullptr;

    vk::Extent2D RenderArea = { 1280, 720 };
};