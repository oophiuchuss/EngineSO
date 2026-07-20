module;

#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <stdexcept>
#include <cstring>

module GPUSceneBuffer;

import VulkanUtils;
import GPUSceneData;

GPUSceneBuffer::GPUSceneBuffer(
    const vk::raii::Device& InDevice,
    const vk::raii::PhysicalDevice& InPhysicalDevice,
	uint32_t InFramesInFlight,
    uint32_t InMaxObjects,
    uint32_t InMaxMaterials) :
    Device(InDevice),
    PhysicalDevice(InPhysicalDevice),
    MaxObjects(InMaxObjects),
    MaxMaterials(InMaxMaterials),
	FramesInFlight(InFramesInFlight)
{
    if (FramesInFlight == 0)
    {
        throw std::invalid_argument("GPUSceneBuffer: FramesInFlight must be greater than zero");
    }

    CreateBuffers();
    CreateDescriptor();
}

GPUSceneBuffer::~GPUSceneBuffer()
{
    // Persistently mapped allocations must be unmapped before DeviceMemory releases them.
    for (FrameResources& Frame : Frames)
    {
        if (Frame.ObjectMappedMemory)
        {
            Frame.ObjectMemory.unmapMemory();
            Frame.ObjectMappedMemory = nullptr;
        }

        if (Frame.MaterialMappedMemory)
        {
            Frame.MaterialMemory.unmapMemory();
            Frame.MaterialMappedMemory = nullptr;
        }
    }
}

void GPUSceneBuffer::CreateBuffers()
{
    Frames.resize(FramesInFlight);

    const vk::DeviceSize ObjectBufferSize = sizeof(ObjectData) * MaxObjects;

    const vk::DeviceSize MaterialBufferSize = sizeof(MaterialData) * MaxMaterials;
    for (FrameResources& Frame : Frames)
    {
        vk::BufferCreateInfo ObjectBufferInfo(
            {}, ObjectBufferSize,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::SharingMode::eExclusive);

        Frame.ObjectBuffer = Device.createBuffer(ObjectBufferInfo);

        auto ObjMemReqs = Frame.ObjectBuffer.getMemoryRequirements();

        const uint32_t ObjMemTypeIndex = VulkanUtils::FindMemoryType(
            PhysicalDevice, ObjMemReqs.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

        Frame.ObjectMemory = Device.allocateMemory(vk::MemoryAllocateInfo(ObjMemReqs.size, ObjMemTypeIndex));

        Frame.ObjectBuffer.bindMemory(*Frame.ObjectMemory, 0);
        Frame.ObjectMappedMemory = Frame.ObjectMemory.mapMemory(0, ObjectBufferSize);

        vk::BufferCreateInfo MaterialBufferInfo(
            {}, MaterialBufferSize,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::SharingMode::eExclusive);

        Frame.MaterialBuffer = Device.createBuffer(MaterialBufferInfo);

        auto MatMemReqs = Frame.MaterialBuffer.getMemoryRequirements();

        const uint32_t MatMemTypeIndex = VulkanUtils::FindMemoryType(
            PhysicalDevice, MatMemReqs.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

        Frame.MaterialMemory = Device.allocateMemory(vk::MemoryAllocateInfo(MatMemReqs.size, MatMemTypeIndex));

        Frame.MaterialBuffer.bindMemory(*Frame.MaterialMemory, 0);
        Frame.MaterialMappedMemory = Frame.MaterialMemory.mapMemory(0, MaterialBufferSize);

    }
}

void GPUSceneBuffer::CreateDescriptor()
{
    // Binding 0 = ObjectData[], Binding 1 = MaterialData[]
    std::array<vk::DescriptorSetLayoutBinding, 2> Bindings = { {
        { 0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment },
        { 1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment }
    } };

    vk::DescriptorSetLayoutCreateInfo LayoutInfo({}, Bindings);
    DescriptorSetLayout = Device.createDescriptorSetLayout(LayoutInfo);

    // Every frame owns one descriptor set containing two storage-buffer descriptors.
    vk::DescriptorPoolSize PoolSize(vk::DescriptorType::eStorageBuffer, FramesInFlight * 2);
    vk::DescriptorPoolCreateInfo PoolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        FramesInFlight, PoolSize);

    DescriptorPool = Device.createDescriptorPool(PoolInfo);

    std::vector<vk::DescriptorSetLayout> Layouts(FramesInFlight, *DescriptorSetLayout);

    vk::DescriptorSetAllocateInfo AllocateInfo(*DescriptorPool, Layouts);
    std::vector<vk::raii::DescriptorSet> DescriptorSets = Device.allocateDescriptorSets(AllocateInfo);

    for (uint32_t FrameIndex = 0; FrameIndex < FramesInFlight; FrameIndex++)
    {
        FrameResources& Frame = Frames[FrameIndex];

        Frame.DescriptorSet = std::move(DescriptorSets[FrameIndex]);

        vk::DescriptorBufferInfo ObjectBufferInfo(*Frame.ObjectBuffer, 0, sizeof(ObjectData) * MaxObjects);

        vk::DescriptorBufferInfo MaterialBufferInfo(*Frame.MaterialBuffer, 0, sizeof(MaterialData) * MaxMaterials);

        std::array<vk::WriteDescriptorSet, 2> Writes = { {
            { *Frame.DescriptorSet, 0, 0, vk::DescriptorType::eStorageBuffer, nullptr, ObjectBufferInfo },
            { *Frame.DescriptorSet, 1, 0, vk::DescriptorType::eStorageBuffer, nullptr, MaterialBufferInfo }
        } };
        
        Device.updateDescriptorSets(Writes, {});
    }
}

void GPUSceneBuffer::Update(
    uint32_t FrameIndex,
    const std::vector<ObjectData>& Objects,
    const std::vector<MaterialData>& Materials)
{
    if (Objects.size() > MaxObjects)
    {
        throw std::runtime_error("GPUSceneBuffer: object count exceeds MaxObjects");
    }

    if (Materials.size() > MaxMaterials)
    {
        throw std::runtime_error("GPUSceneBuffer: material count exceeds MaxMaterials");
    }

    FrameResources& Frame = GetFrameResources(FrameIndex);

    if (!Objects.empty())
    {
        std::memcpy(Frame.ObjectMappedMemory, Objects.data(), Objects.size() * sizeof(ObjectData));
    }

    if (!Materials.empty())
    {
        std::memcpy(Frame.MaterialMappedMemory, Materials.data(), Materials.size() * sizeof(MaterialData));
    }
}

const vk::raii::DescriptorSet& GPUSceneBuffer::GetDescriptorSet(uint32_t FrameIndex) const
{
    return GetFrameResources(FrameIndex).DescriptorSet;
}

GPUSceneBuffer::FrameResources& GPUSceneBuffer::GetFrameResources(uint32_t FrameIndex)
{
    if (FrameIndex >= Frames.size())
    {
        throw std::out_of_range("GPUSceneBuffer: frame index is out of range");
    }

    return Frames[FrameIndex];
}

const GPUSceneBuffer::FrameResources& GPUSceneBuffer::GetFrameResources(uint32_t FrameIndex) const
{
    if (FrameIndex >= Frames.size())
    {
        throw std::out_of_range("GPUSceneBuffer: frame index is out of range");
    }

    return Frames[FrameIndex];
}