module;

#include <vulkan/vulkan_raii.hpp>
#include <vector>

export module LightBuffer;

import GPULightData;

export class LightBuffer
{
public:
    LightBuffer(
        const vk::raii::Device& Device,
        const vk::raii::PhysicalDevice& PhysicalDevice,
        uint32_t  MaxLights = 128);

    ~LightBuffer() = default;

    // Upload an array of GPULightData to the GPU buffer.
    // Automatically writes the light count header followed by the LightData entries.
    void Update(const std::vector<GPULightData>& Lights);

    vk::DescriptorSet GetDescriptorSet() const { return *DescriptorSet; }
    vk::DescriptorSetLayout GetDescriptorSetLayout() const { return *DescriptorLayout; }

private:
    void CreateBuffer();
    void CreateDescriptorLayout();
    void CreateDescriptorPool();
    void CreateDescriptorSet();

    const vk::raii::Device& Device;
    const vk::raii::PhysicalDevice& PhysicalDevice;
    uint32_t MaxLights;

    // Pre‑allocated to maximum size, host‑visible & coherent
    vk::DeviceSize BufferSize;

    vk::raii::Buffer Buffer = nullptr;
    vk::raii::DeviceMemory BufferMemory = nullptr;
    void* MappedMemory = nullptr;

    vk::raii::DescriptorSetLayout DescriptorLayout = nullptr;
    vk::raii::DescriptorPool DescriptorPool = nullptr;
    vk::raii::DescriptorSet DescriptorSet = nullptr;
};