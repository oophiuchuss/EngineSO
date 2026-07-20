module;

#include <vulkan/vulkan_raii.hpp>
#include <vector>

export module GPUSceneBuffer;

import GPUSceneData;

// Owns both ObjectData[] and MaterialData[] SSBOs plus their shared descriptor set
export class GPUSceneBuffer
{
public:
    GPUSceneBuffer(
        const vk::raii::Device& Device,
        const vk::raii::PhysicalDevice& PhysicalDevice,
        uint32_t FramesInFlight,
        uint32_t MaxObjects,
        uint32_t MaxMaterials);

    ~GPUSceneBuffer();

    // Throws if Objects.size() > MaxObjects or Materials.size() > MaxMaterials
    void Update(uint32_t FrameIndex, const std::vector<ObjectData>& Objects, const std::vector<MaterialData>& Materials);

    const vk::raii::DescriptorSet& GetDescriptorSet(uint32_t FrameIndex) const;
    const vk::raii::DescriptorSetLayout& GetDescriptorSetLayout() const { return DescriptorSetLayout; }

    uint32_t GetMaxObjects() const { return MaxObjects; }
    uint32_t GetMaxMaterials() const { return MaxMaterials; }

private:
    struct FrameResources
    {
        // Memory is declared before the corresponding buffer so reverse member
        // destruction destroys the buffer before freeing its memory.
        vk::raii::DeviceMemory ObjectMemory = nullptr;
        vk::raii::Buffer ObjectBuffer = nullptr;
        void* ObjectMappedMemory = nullptr;

        vk::raii::DeviceMemory MaterialMemory = nullptr;
        vk::raii::Buffer MaterialBuffer = nullptr;
        void* MaterialMappedMemory = nullptr;

        // Destroy the descriptor set before the buffers it references.
        vk::raii::DescriptorSet DescriptorSet = nullptr;
    };

    void CreateBuffers();
    void CreateDescriptor();

    FrameResources& GetFrameResources(uint32_t FrameIndex);
    const FrameResources& GetFrameResources(uint32_t FrameIndex) const;

    const vk::raii::Device& Device;
    const vk::raii::PhysicalDevice& PhysicalDevice;

    uint32_t MaxObjects;
    uint32_t MaxMaterials;
    uint32_t FramesInFlight;

    vk::raii::DescriptorSetLayout DescriptorSetLayout = nullptr;
    vk::raii::DescriptorPool DescriptorPool = nullptr;
    std::vector<FrameResources> Frames;
};