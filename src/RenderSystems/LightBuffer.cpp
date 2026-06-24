module;

#include <vulkan/vulkan_raii.hpp>
#include <cstring>
#include <stdexcept>

module LightBuffer;

import LightData;
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
    BufferSize = 16 + sizeof(LightData) * MaxLights;

    vk::BufferCreateInfo BufferInfo(
        {},
        BufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer,
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


void LightBuffer::Update(const std::vector<LightData>& Lights)
{
    if (Lights.size() > MaxLights)
        throw std::runtime_error("LightBuffer: light count exceeds MaxLights");

    uint32_t Count = static_cast<uint32_t>(Lights.size());
    vk::DeviceSize HeaderSize = 16; // count + padding
    vk::DeviceSize DataSize = Count * sizeof(LightData);

    // Write header (light count) directly to mapped memory
    std::memcpy(MappedMemory, &Count, sizeof(Count));

    // Write LightData array right after the header
    std::memcpy(
        static_cast<uint8_t*>(MappedMemory) + HeaderSize,
        Lights.data(),
        static_cast<size_t>(DataSize));
}

void LightBuffer::CreateDescriptorLayout()
{
    vk::DescriptorSetLayoutBinding Binding(
        0,                                              // binding
        vk::DescriptorType::eStorageBuffer,
        1,
        vk::ShaderStageFlagBits::eFragment);

    vk::DescriptorSetLayoutCreateInfo LayoutInfo({}, 1, &Binding);
    DescriptorLayout = Device.createDescriptorSetLayout(LayoutInfo);
}

void LightBuffer::CreateDescriptorPool()
{
    vk::DescriptorPoolSize PoolSize(vk::DescriptorType::eStorageBuffer, 1);
    vk::DescriptorPoolCreateInfo PoolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        1, PoolSize);

    DescriptorPool = Device.createDescriptorPool(PoolInfo);
}

void LightBuffer::CreateDescriptorSet()
{
    vk::DescriptorSetAllocateInfo AllocInfo(*DescriptorPool, *DescriptorLayout);
    DescriptorSet = std::move(Device.allocateDescriptorSets(AllocInfo).front());

    vk::DescriptorBufferInfo BufferInfo(*Buffer, 0, BufferSize);
    vk::WriteDescriptorSet Write(
        *DescriptorSet,
        0, 0, 1,
        vk::DescriptorType::eStorageBuffer,
        nullptr,
        &BufferInfo);
    Device.updateDescriptorSets(Write, {});
}