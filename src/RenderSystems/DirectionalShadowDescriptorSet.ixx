module;

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module DirectionalShadowDescriptorSet;

import DirectionalShadowSettings;
import DirectionalShadowMap;

export class DirectionalShadowDescriptorSet
{
public:
    DirectionalShadowDescriptorSet(
        const vk::raii::Device& InDevice,
        const vk::raii::PhysicalDevice& InPhysicalDevice,
        uint32_t InFramesInFlight);

    ~DirectionalShadowDescriptorSet();

    void Initialize(const DirectionalShadowMap& ShadowMap);

    void UpdateUniform(uint32_t FrameIndex, const DirectionalShadowUniformData& Data);

    // Used after shadow-map recreation.
    void UpdateImage(uint32_t FrameIndex, vk::ImageView ImageView);

    const vk::raii::DescriptorSet& GetDescriptorSet(uint32_t FrameIndex) const { return Frames.at(FrameIndex).DescriptorSet; }

    const vk::raii::DescriptorSetLayout& GetDescriptorSetLayout() const { return DescriptorSetLayout; }

private:
    struct FrameResources
    {
        // Reverse destruction keeps the descriptor and buffer valid
        // until before their backing memory is released.
        vk::raii::DeviceMemory UniformMemory = nullptr;
        vk::raii::Buffer UniformBuffer = nullptr;
        void* MappedMemory = nullptr;

        vk::raii::DescriptorSet DescriptorSet = nullptr;
    };

    void CreateUniformBuffer(FrameResources& Frame);
    void CreateDescriptors();

    const vk::raii::Device& Device;
    const vk::raii::PhysicalDevice& PhysicalDevice;

    uint32_t FramesInFlight = 0;

    vk::raii::Sampler Sampler = nullptr;
    vk::raii::DescriptorSetLayout DescriptorSetLayout = nullptr;
    vk::raii::DescriptorPool DescriptorPool = nullptr;

    std::vector<FrameResources> Frames;
};