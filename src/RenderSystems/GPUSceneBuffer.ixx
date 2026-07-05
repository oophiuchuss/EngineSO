module;

#include <vulkan/vulkan_raii.hpp>
#include <vector>

export module GPUSceneBuffer;

import GPUSceneData;

// Owns both ObjectData[] and MaterialData[] SSBOs plus their shared descriptor set
// TODO: upgrade from single buffer per array to double buffering
export class GPUSceneBuffer
{
public:
    GPUSceneBuffer(
        const vk::raii::Device& Device,
        const vk::raii::PhysicalDevice& PhysicalDevice,
        uint32_t MaxObjects,
        uint32_t MaxMaterials);

    ~GPUSceneBuffer() = default;

    // Throws if Objects.size() > MaxObjects or Materials.size() > MaxMaterials
    void Update(const std::vector<ObjectData>& Objects, const std::vector<MaterialData>& Materials);

    const vk::raii::DescriptorSet& GetDescriptorSet() const { return DescriptorSet; }
    const vk::raii::DescriptorSetLayout& GetDescriptorSetLayout() const { return DescriptorSetLayout; }

    uint32_t GetMaxObjects() const { return MaxObjects; }
    uint32_t GetMaxMaterials() const { return MaxMaterials; }

private:
    void CreateBuffers();
    void CreateDescriptor();

    const vk::raii::Device& Device;
    const vk::raii::PhysicalDevice& PhysicalDevice;

    uint32_t MaxObjects;
    uint32_t MaxMaterials;

    // Object buffer
    vk::raii::Buffer ObjectBuffer = nullptr;
    vk::raii::DeviceMemory ObjectMemory = nullptr;
    void* ObjectMappedMemory = nullptr;

    // Material buffer
    vk::raii::Buffer MaterialBuffer = nullptr;
    vk::raii::DeviceMemory MaterialMemory = nullptr;
    void* MaterialMappedMemory = nullptr;

    vk::raii::DescriptorPool DescriptorPool = nullptr;
    vk::raii::DescriptorSetLayout DescriptorSetLayout = nullptr;
    vk::raii::DescriptorSet DescriptorSet = nullptr;
};