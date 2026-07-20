module;

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

module LightBuffer;

import GPULightData;
import VulkanUtils;

LightBuffer::LightBuffer(
    const vk::raii::Device& InDevice,
    const vk::raii::PhysicalDevice& InPhysicalDevice,
    uint32_t InFramesInFlight,
    uint32_t InMaxLights) :
    Device(InDevice),
    PhysicalDevice(InPhysicalDevice),
    FramesInFlight(InFramesInFlight),
    MaxLights(InMaxLights)
{
    if (FramesInFlight == 0)
    {
        throw std::invalid_argument("LightBuffer: FramesInFlight must be greater than zero");
    }

    CalculateBufferLayout();
    CreateBuffer();
    CreateDescriptorLayout();
    CreateDescriptorPool();
    CreateDescriptorSet();
}

LightBuffer::~LightBuffer()
{
    for (FrameResources& Frame : Frames)
    {
        if (Frame.MappedMemory)
        {
            Frame.Memory.unmapMemory();
            Frame.MappedMemory = nullptr;
        }
    }
}

void LightBuffer::Update(uint32_t FrameIndex, const std::vector<GPULightData>& Lights)
{
    if (Lights.size() > MaxLights)
    {
        throw std::runtime_error("LightBuffer: light count exceeds MaxLights");
    }

    FrameResources& Frame = GetFrameResources(FrameIndex);

    const uint32_t LightCount = static_cast<uint32_t>(Lights.size());

    std::memcpy(Frame.MappedMemory, &LightCount, sizeof(LightCount));

    if (!Lights.empty())
    {
        const vk::DeviceSize DataSize = sizeof(GPULightData) * Lights.size();

        std::memcpy(static_cast<uint8_t*>(Frame.MappedMemory) + LightDataOffset, Lights.data(), static_cast<size_t>(DataSize));
    }
}

const vk::raii::DescriptorSet& LightBuffer::GetDescriptorSet(uint32_t FrameIndex) const
{
    return GetFrameResources(FrameIndex).DescriptorSet;
}

void LightBuffer::CalculateBufferLayout()
{
    const vk::PhysicalDeviceProperties Properties = PhysicalDevice.getProperties();

    const vk::DeviceSize StorageAlignment = std::max<vk::DeviceSize>(1, Properties.limits.minStorageBufferOffsetAlignment);

    // Round HeaderSize upward to the next legal storage-buffer offset.
    LightDataOffset = ((HeaderSize + StorageAlignment - 1) / StorageAlignment) * StorageAlignment;

    BufferSize = LightDataOffset + sizeof(GPULightData) * MaxLights;
}

void LightBuffer::CreateBuffer()
{
    Frames.resize(FramesInFlight);

    for (FrameResources& Frame : Frames)
    {
        vk::BufferCreateInfo BufferInfo(
            {},
            BufferSize,
            vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::SharingMode::eExclusive);

        Frame.Buffer = Device.createBuffer(BufferInfo);

        auto MemReqs = Frame.Buffer.getMemoryRequirements();

        uint32_t MemTypeIndex = VulkanUtils::FindMemoryType(
            PhysicalDevice,
            MemReqs.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

        Frame.Memory = Device.allocateMemory(vk::MemoryAllocateInfo(MemReqs.size, MemTypeIndex));

        Frame.Buffer.bindMemory(*Frame.Memory, 0);

        Frame.MappedMemory = Frame.Memory.mapMemory(0, BufferSize);
        std::memset(Frame.MappedMemory, 0, static_cast<size_t>(BufferSize));
    }
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
            { vk::DescriptorType::eUniformBuffer, FramesInFlight },
            { vk::DescriptorType::eStorageBuffer, FramesInFlight },
        }
    };

    vk::DescriptorPoolCreateInfo PoolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        FramesInFlight,
        PoolSizes);

    DescriptorPool = Device.createDescriptorPool(PoolInfo);
}

void LightBuffer::CreateDescriptorSet()
{
    std::vector<vk::DescriptorSetLayout> Layouts(
        FramesInFlight,
        *DescriptorLayout);

    vk::DescriptorSetAllocateInfo AllocateInfo(
        *DescriptorPool,
        Layouts);

    std::vector<vk::raii::DescriptorSet> DescriptorSets = Device.allocateDescriptorSets(AllocateInfo);

    for (uint32_t FrameIndex = 0; FrameIndex < FramesInFlight; FrameIndex++)
    {
        FrameResources& Frame = Frames[FrameIndex];

        Frame.DescriptorSet = std::move(DescriptorSets[FrameIndex]);

        // Binding 0 – header (count + padding)
        vk::DescriptorBufferInfo HeaderInfo(*Frame.Buffer, 0, HeaderSize);
        // Binding 1 – LightData[] starting at offset
        vk::DescriptorBufferInfo DataInfo(*Frame.Buffer, LightDataOffset, sizeof(GPULightData) * MaxLights);

        std::array<vk::WriteDescriptorSet, 2> Writes = { {
            { *Frame.DescriptorSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer,
              nullptr, &HeaderInfo },
            { *Frame.DescriptorSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer,
              nullptr, &DataInfo },
        } };

        Device.updateDescriptorSets(Writes, {});
    }
}

LightBuffer::FrameResources& LightBuffer::GetFrameResources(uint32_t FrameIndex)
{
    if (FrameIndex >= Frames.size())
    {
        throw std::out_of_range("LightBuffer: frame index is out of range");
    }

    return Frames[FrameIndex];
}

const LightBuffer::FrameResources& LightBuffer::GetFrameResources(uint32_t FrameIndex) const
{
    if (FrameIndex >= Frames.size())
    {
        throw std::out_of_range("LightBuffer: frame index is out of range");
    }

    return Frames[FrameIndex];
}
