module;

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

module FrameUniform;

import VulkanUtils;

FrameUniformBuffer::FrameUniformBuffer(
    const vk::raii::Device& InDevice,
    const vk::raii::PhysicalDevice& InPhysicalDevice,
    uint32_t InFramesInFlight) :
    Device(InDevice),
    PhysicalDevice(InPhysicalDevice),
    FramesInFlight(InFramesInFlight)
{
    if (FramesInFlight == 0)
    {
        throw std::invalid_argument("FrameUniformBuffer: FramesInFlight must be greater than zero");
    }

    CreateBuffers();
    CreateDescriptors();
}

FrameUniformBuffer::~FrameUniformBuffer()
{
    for (FrameResources& Frame : Frames)
    {
        DestroyMappedBuffer(Frame.Camera);
        DestroyMappedBuffer(Frame.Environment);
    }
}

void FrameUniformBuffer::Update(uint32_t FrameIndex, const FrameUniformData& Data)
{
    FrameResources& Frame = GetFrameResources(FrameIndex);

    std::memcpy(Frame.Camera.MappedMemory, &Data.Camera, sizeof(CameraUniformData));
    std::memcpy(Frame.Environment.MappedMemory, &Data.Environment, sizeof(EnvironmentUniformData));

    LastData = Data;
}

const vk::raii::DescriptorSet& FrameUniformBuffer::GetDescriptorSet(uint32_t FrameIndex) const
{
    return GetFrameResources(FrameIndex).DescriptorSet;
}

void FrameUniformBuffer::CreateBuffers()
{
    Frames.resize(FramesInFlight);

    for (FrameResources& Frame : Frames)
    {
        CreateUniformBuffer(sizeof(CameraUniformData), Frame.Camera);

        CreateUniformBuffer(sizeof(EnvironmentUniformData), Frame.Environment);
    }
}

void FrameUniformBuffer::CreateUniformBuffer(vk::DeviceSize Size, UniformBufferResource& Output)
{
    vk::BufferCreateInfo BufferInfo(
        {},
        Size,
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::SharingMode::eExclusive);

    Output.Buffer = Device.createBuffer(BufferInfo);

    const vk::MemoryRequirements MemoryRequirements = Output.Buffer.getMemoryRequirements();

    const uint32_t MemoryTypeIndex = VulkanUtils::FindMemoryType(
            PhysicalDevice,
            MemoryRequirements.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

    vk::MemoryAllocateInfo AllocateInfo(MemoryRequirements.size, MemoryTypeIndex);

    Output.Memory = Device.allocateMemory(AllocateInfo);

    Output.Buffer.bindMemory(*Output.Memory, 0);

    Output.MappedMemory = Output.Memory.mapMemory(0, Size);
}

void FrameUniformBuffer::DestroyMappedBuffer(UniformBufferResource& Resource)
{
    if (!Resource.MappedMemory)
    {
        return;
    }

    Resource.Memory.unmapMemory();
    Resource.MappedMemory = nullptr;
}

void FrameUniformBuffer::CreateDescriptors()
{
    std::array<vk::DescriptorSetLayoutBinding, 2> Bindings = { {
        {
            0,
            vk::DescriptorType::eUniformBuffer,
            1,
            vk::ShaderStageFlagBits::eVertex |
            vk::ShaderStageFlagBits::eFragment
        },
        {
            1,
            vk::DescriptorType::eUniformBuffer,
            1,
            vk::ShaderStageFlagBits::eFragment
        }
    } };

    vk::DescriptorSetLayoutCreateInfo LayoutInfo({}, Bindings);

    DescriptorSetLayout = Device.createDescriptorSetLayout(LayoutInfo);

    vk::DescriptorPoolSize PoolSize(vk::DescriptorType::eUniformBuffer, FramesInFlight * 2);

    vk::DescriptorPoolCreateInfo PoolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, FramesInFlight, PoolSize);

    DescriptorPool = Device.createDescriptorPool(PoolInfo);

    std::vector<vk::DescriptorSetLayout> Layouts(FramesInFlight, *DescriptorSetLayout);

    vk::DescriptorSetAllocateInfo AllocateInfo(*DescriptorPool, Layouts);

    std::vector<vk::raii::DescriptorSet> DescriptorSets = Device.allocateDescriptorSets(AllocateInfo);

    for (uint32_t FrameIndex = 0; FrameIndex < FramesInFlight; FrameIndex++)
    {
        FrameResources& Frame = Frames[FrameIndex];

        Frame.DescriptorSet = std::move(DescriptorSets[FrameIndex]);

        vk::DescriptorBufferInfo CameraBufferInfo(
            *Frame.Camera.Buffer,
            0,
            sizeof(CameraUniformData));

        vk::DescriptorBufferInfo EnvironmentBufferInfo(
            *Frame.Environment.Buffer,
            0,
            sizeof(EnvironmentUniformData));

        std::array<vk::WriteDescriptorSet, 2> Writes;

        Writes[0].setDstSet(*Frame.DescriptorSet)
            .setDstBinding(0)
            .setDstArrayElement(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setBufferInfo(CameraBufferInfo);

        Writes[1].setDstSet(*Frame.DescriptorSet)
            .setDstBinding(1)
            .setDstArrayElement(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setBufferInfo(EnvironmentBufferInfo);

        Device.updateDescriptorSets(Writes, {});
    }
}

FrameUniformBuffer::FrameResources& FrameUniformBuffer::GetFrameResources(uint32_t FrameIndex)
{
    if (FrameIndex >= Frames.size())
    {
        throw std::out_of_range("FrameUniformBuffer: frame index is out of range");
    }

    return Frames[FrameIndex];
}

const FrameUniformBuffer::FrameResources& FrameUniformBuffer::GetFrameResources(uint32_t FrameIndex) const
{
    if (FrameIndex >= Frames.size())
    {
        throw std::out_of_range("FrameUniformBuffer: frame index is out of range");
    }

    return Frames[FrameIndex];
}