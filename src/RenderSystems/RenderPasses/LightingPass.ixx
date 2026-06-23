module;

#include <string>
#include <vulkan/vulkan_raii.hpp>

export module LightingPass;

import RenderPassBase;
import FrameData;
import CameraUniform;
import LightBuffer;
import GBufferDescriptorSet;

export class LightingPass : public RenderPassBase
{
public:
    explicit LightingPass(
        std::string InName,
        std::string InOutputColorResourceName,
        std::string InGBufferAlbedoResourceName,
        std::string InGBufferNormalResourceName,
        std::string InGBufferMetalRoughResourceName,
        std::string InGBufferEmissiveResourceName,
        std::string InGBufferDepthResourceName,
        CameraUniformBuffer* InCameraUBO,
        LightBuffer* InLightBuffer
        GBufferDescriptorSet* InGBufferDescSet);

protected:
    void BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;
    void ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;
    void EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;

private:
    std::string OutputColorResourceName;
    std::string GBufferAlbedoResourceName;
    std::string GBufferNormalResourceName;
    std::string GBufferMetalRoughResourceName;
    std::string GBufferEmissiveResourceName;
    std::string GBufferDepthResourceName;

    CameraUniformBuffer* CameraUBOPtr = nullptr;
    LightBuffer* LightBufferPtr = nullptr;
    GBufferDescriptorSet* GBufferDescSetPtr = nullptr;
};