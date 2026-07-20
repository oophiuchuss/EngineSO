module;

#include <cstdint>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

export module LightBuffer;

import GPULightData;

export class LightBuffer
{
public:
    LightBuffer(
        const vk::raii::Device& Device,
        const vk::raii::PhysicalDevice& PhysicalDevice,
        uint32_t FramesInFlight,
        uint32_t MaxLights = 128);

    ~LightBuffer();

    // Automatically writes the light count header followed by the LightData entries.
    void Update(uint32_t FrameIndex, const std::vector<GPULightData>& Lights);

    const vk::raii::DescriptorSet& GetDescriptorSet(uint32_t FrameIndex) const;
    const vk::raii::DescriptorSetLayout& GetDescriptorSetLayout() const { return DescriptorLayout; }

private:
    struct FrameResources
    {
        vk::raii::DeviceMemory Memory = nullptr;
        vk::raii::Buffer Buffer = nullptr;
        void* MappedMemory = nullptr;

        vk::raii::DescriptorSet DescriptorSet = nullptr;
    };

    void CalculateBufferLayout();
    void CreateBuffer();
    void CreateDescriptorLayout();
    void CreateDescriptorPool();
    void CreateDescriptorSet();

    FrameResources& GetFrameResources(uint32_t FrameIndex);
    const FrameResources& GetFrameResources(uint32_t FrameIndex) const;

    const vk::raii::Device& Device;
    const vk::raii::PhysicalDevice& PhysicalDevice;

    uint32_t FramesInFlight;
    uint32_t MaxLights;

    static constexpr vk::DeviceSize HeaderSize = 16;

    vk::DeviceSize LightDataOffset = 0;
    vk::DeviceSize BufferSize = 0;

    vk::raii::DescriptorSetLayout DescriptorLayout = nullptr;
    vk::raii::DescriptorPool DescriptorPool = nullptr;
    std::vector<FrameResources> Frames;
};