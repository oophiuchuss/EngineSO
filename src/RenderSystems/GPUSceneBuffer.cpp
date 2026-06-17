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
    uint32_t InMaxObjects,
    uint32_t InMaxMaterials) :
    Device(InDevice),
    PhysicalDevice(InPhysicalDevice),
    MaxObjects(InMaxObjects),
    MaxMaterials(InMaxMaterials)
{
    CreateBuffers();
    CreateDescriptor();
}

void GPUSceneBuffer::CreateBuffers()
{
    // Object buffer
    vk::DeviceSize ObjectBufferSize = sizeof(ObjectData) * MaxObjects;

    vk::BufferCreateInfo ObjectBufferInfo(
        {}, ObjectBufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::SharingMode::eExclusive);

    ObjectBuffer = Device.createBuffer(ObjectBufferInfo);

    auto ObjMemReqs = ObjectBuffer.getMemoryRequirements();
    uint32_t ObjMemTypeIndex = VulkanUtils::FindMemoryType(
        PhysicalDevice, ObjMemReqs.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent);

    ObjectMemory = Device.allocateMemory(vk::MemoryAllocateInfo(ObjMemReqs.size, ObjMemTypeIndex));

    ObjectBuffer.bindMemory(*ObjectMemory, 0);
    ObjectMappedMemory = ObjectMemory.mapMemory(0, ObjectBufferSize);

    // Material buffer
    vk::DeviceSize MaterialBufferSize = sizeof(MaterialData) * MaxMaterials;

    vk::BufferCreateInfo MaterialBufferInfo(
        {}, MaterialBufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::SharingMode::eExclusive);

    MaterialBuffer = Device.createBuffer(MaterialBufferInfo);

    auto MatMemReqs = MaterialBuffer.getMemoryRequirements();
    uint32_t MatMemTypeIndex = VulkanUtils::FindMemoryType(
        PhysicalDevice, MatMemReqs.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent);

    MaterialMemory = Device.allocateMemory(vk::MemoryAllocateInfo(MatMemReqs.size, MatMemTypeIndex));

    MaterialBuffer.bindMemory(*MaterialMemory, 0);
    MaterialMappedMemory = MaterialMemory.mapMemory(0, MaterialBufferSize);
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

    vk::DescriptorPoolSize PoolSize(vk::DescriptorType::eStorageBuffer, 2);
    vk::DescriptorPoolCreateInfo PoolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        1, PoolSize);

    DescriptorPool = Device.createDescriptorPool(PoolInfo);

    vk::DescriptorSetAllocateInfo AllocInfo(*DescriptorPool, *DescriptorSetLayout);
    DescriptorSet = std::move(Device.allocateDescriptorSets(AllocInfo).front());

    vk::DescriptorBufferInfo ObjectBufferInfo(*ObjectBuffer, 0, sizeof(ObjectData) * MaxObjects);

    vk::DescriptorBufferInfo MaterialBufferInfo(*MaterialBuffer, 0, sizeof(MaterialData) * MaxMaterials);

    std::array<vk::WriteDescriptorSet, 2> Writes = { {
        { *DescriptorSet, 0, 0, vk::DescriptorType::eStorageBuffer, nullptr, ObjectBufferInfo },
        { *DescriptorSet, 1, 0, vk::DescriptorType::eStorageBuffer, nullptr, MaterialBufferInfo }
    } };

    Device.updateDescriptorSets(Writes, {});
}

void GPUSceneBuffer::Update(
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

    if (!Objects.empty())
    {
        std::memcpy(ObjectMappedMemory, Objects.data(), Objects.size() * sizeof(ObjectData));
    }

    if (!Materials.empty())
    {
        std::memcpy(MaterialMappedMemory, Materials.data(), Materials.size() * sizeof(MaterialData));
    }
}