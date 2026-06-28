module;

#include <vulkan/vulkan_raii.hpp>
#include <cstring>
#include <stdexcept>

module LightBuffer;

import GPULightData;
import VulkanUtils;

LightBuffer::LightBuffer(
    const vk::raii::Device& InDevice,
    const vk::raii::PhysicalDevice& InPhysicalDevice,
    uint32_t InMaxLights) :
    Device(InDevice),
    PhysicalDevice(InPhysicalDevice),
    MaxLights(InMaxLights)
{
    CreateBuffer();
    CreateDescriptorLayout();
    CreateDescriptorPool();
    CreateDescriptorSet();
}

void LightBuffer::CreateBuffer()
{
    // Layout: [uint32 LightCount][3 x uint32 padding][LightData[]]
    BufferSize = 16 + sizeof(GPULightData) * MaxLights;

    vk::BufferCreateInfo BufferInfo(
        {},
        BufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer |
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::SharingMode::eExclusive);

    Buffer = Device.createBuffer(BufferInfo);

    auto MemReqs = Buffer.getMemoryRequirements();
    uint32_t MemTypeIndex = VulkanUtils::FindMemoryType(
        PhysicalDevice,
        MemReqs.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent);

    BufferMemory = Device.allocateMemory(
        vk::MemoryAllocateInfo(MemReqs.size, MemTypeIndex));
    Buffer.bindMemory(*BufferMemory, 0);

    MappedMemory = BufferMemory.mapMemory(0, BufferSize);
    std::memset(MappedMemory, 0, static_cast<size_t>(BufferSize));
}


void LightBuffer::Update(const std::vector<GPULightData>& Lights)
{
    if (Lights.size() > MaxLights)
        throw std::runtime_error("LightBuffer: light count exceeds MaxLights");

    uint32_t Count = static_cast<uint32_t>(Lights.size());
    vk::DeviceSize HeaderSize = 16; // count + padding
    vk::DeviceSize DataSize = Count * sizeof(GPULightData);

    // Write header (light count) directly to mapped memory
    std::memcpy(MappedMemory, &Count, sizeof(Count));

    // Write GPULightData array right after the header
    std::memcpy(
        static_cast<uint8_t*>(MappedMemory) + HeaderSize,
        Lights.data(),
        static_cast<size_t>(DataSize));
}

void LightBuffer::CreateDescriptorLayout()
{
    std::array<vk::DescriptorSetLayoutBinding, 2> Bindings = {
        {
            // Binding 0 – light count (uniform buffer, just the 16‑byte header)
            { 0, vk::DescriptorType::eUniformBuffer, 1,
              vk::ShaderStageFlagBits::eFragment },
            
              // Binding 1 – light data array (storage buffer, starts after header)
            { 1, vk::DescriptorType::eStorageBuffer, 1,
              vk::ShaderStageFlagBits::eFragment },
        }
    };

    vk::DescriptorSetLayoutCreateInfo LayoutInfo({}, Bindings);
    DescriptorLayout = Device.createDescriptorSetLayout(LayoutInfo);
}

void LightBuffer::CreateDescriptorPool()
{
    std::array<vk::DescriptorPoolSize, 2> PoolSizes = { 
        {
            { vk::DescriptorType::eUniformBuffer, 1 },
            { vk::DescriptorType::eStorageBuffer, 1 },
        }
    };

    vk::DescriptorPoolCreateInfo PoolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        1, PoolSizes);

    DescriptorPool = Device.createDescriptorPool(PoolInfo);
}

void LightBuffer::CreateDescriptorSet()
{
    vk::DescriptorSetAllocateInfo AllocInfo(*DescriptorPool, 1, &*DescriptorLayout);
    DescriptorSet = std::move(Device.allocateDescriptorSets(AllocInfo).front());

    // Binding 0 – 16‑byte header (count + padding)
    vk::DescriptorBufferInfo HeaderInfo(*Buffer, 0, 16);
    // Binding 1 – LightData[] starting at offset 16
    vk::DescriptorBufferInfo DataInfo(*Buffer, 16, sizeof(GPULightData) * MaxLights);

    std::array<vk::WriteDescriptorSet, 2> Writes = { {
        { *DescriptorSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer,
          nullptr, &HeaderInfo },
        { *DescriptorSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer,
          nullptr, &DataInfo },
    } };

    Device.updateDescriptorSets(Writes, {});
}