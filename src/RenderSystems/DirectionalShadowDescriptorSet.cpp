module;

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

module DirectionalShadowDescriptorSet;

import VulkanUtils;

DirectionalShadowDescriptorSet::DirectionalShadowDescriptorSet(
    const vk::raii::Device& InDevice,
    const vk::raii::PhysicalDevice& InPhysicalDevice,
    uint32_t InFramesInFlight) :
    Device(InDevice),
    PhysicalDevice(InPhysicalDevice),
    FramesInFlight(InFramesInFlight)
{
    if (FramesInFlight == 0)
    {
        throw std::invalid_argument("DirectionalShadowDescriptorSet: frames in flight must be greater than zero");
    }
}

DirectionalShadowDescriptorSet::~DirectionalShadowDescriptorSet()
{
    for (FrameResources& Frame : Frames)
    {
        if (Frame.MappedMemory)
        {
            Frame.UniformMemory.unmapMemory();
            Frame.MappedMemory = nullptr;
        }
    }
}

void DirectionalShadowDescriptorSet::Initialize(const DirectionalShadowMap& ShadowMap)
{
    if (ShadowMap.GetResolution() == 0)
    {
        throw std::invalid_argument("DirectionalShadowDescriptorSet: shadow map is not initialized");
    }

    vk::SamplerCreateInfo SamplerInfo;

    SamplerInfo.setMagFilter(vk::Filter::eNearest)
        .setMinFilter(vk::Filter::eNearest)
        .setMipmapMode(vk::SamplerMipmapMode::eNearest)
        .setAddressModeU(vk::SamplerAddressMode::eClampToBorder)
        .setAddressModeV(vk::SamplerAddressMode::eClampToBorder)
        .setAddressModeW(vk::SamplerAddressMode::eClampToBorder)
        .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
        .setCompareEnable(VK_FALSE)
        .setMinLod(0.0f)
        .setMaxLod(0.0f);

    Sampler = Device.createSampler(SamplerInfo);

    const std::array<vk::DescriptorSetLayoutBinding, 2> Bindings =
    { {
        {
            0,
            vk::DescriptorType::eUniformBuffer,
            1,
            vk::ShaderStageFlagBits::eVertex |
            vk::ShaderStageFlagBits::eFragment
        },
        {
            1,
            vk::DescriptorType::eCombinedImageSampler,
            1,
            vk::ShaderStageFlagBits::eFragment
        }
    } };

    vk::DescriptorSetLayoutCreateInfo LayoutInfo;
    LayoutInfo.setBindings(Bindings);

    DescriptorSetLayout = Device.createDescriptorSetLayout(LayoutInfo);

    Frames.resize(FramesInFlight);

    for (FrameResources& Frame : Frames)
    {
        CreateUniformBuffer(Frame);
    }

    CreateDescriptors();

    for (uint32_t FrameIndex = 0; FrameIndex < FramesInFlight; FrameIndex++)
    {
        UpdateImage(FrameIndex, ShadowMap.GetImageView(FrameIndex));
    }
}

void DirectionalShadowDescriptorSet::CreateUniformBuffer(FrameResources& Frame)
{
    vk::BufferCreateInfo BufferInfo;

    BufferInfo.setSize(sizeof(DirectionalShadowUniformData))
        .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
        .setSharingMode(vk::SharingMode::eExclusive);

    Frame.UniformBuffer = Device.createBuffer(BufferInfo);

    const vk::MemoryRequirements MemoryRequirements = Frame.UniformBuffer.getMemoryRequirements();

    const uint32_t MemoryTypeIndex = VulkanUtils::FindMemoryType(
        PhysicalDevice, MemoryRequirements.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent);

    vk::MemoryAllocateInfo AllocationInfo;

    AllocationInfo.setAllocationSize(MemoryRequirements.size)
        .setMemoryTypeIndex(MemoryTypeIndex);

    Frame.UniformMemory = Device.allocateMemory(AllocationInfo);

    Frame.UniformBuffer.bindMemory(*Frame.UniformMemory, 0);

    Frame.MappedMemory = Frame.UniformMemory.mapMemory(0, sizeof(DirectionalShadowUniformData));
}

void DirectionalShadowDescriptorSet::CreateDescriptors()
{
    const std::array<vk::DescriptorPoolSize, 2> PoolSizes =
    { {
        {
            vk::DescriptorType::eUniformBuffer,
            FramesInFlight
        },
        {
            vk::DescriptorType::eCombinedImageSampler,
            FramesInFlight
        }
    } };

    vk::DescriptorPoolCreateInfo PoolInfo;

    PoolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(FramesInFlight)
        .setPoolSizes(PoolSizes);

    DescriptorPool = Device.createDescriptorPool(PoolInfo);

    const std::vector<vk::DescriptorSetLayout> Layouts(FramesInFlight, *DescriptorSetLayout);

    vk::DescriptorSetAllocateInfo AllocateInfo;

    AllocateInfo.setDescriptorPool(*DescriptorPool)
        .setSetLayouts(Layouts);

    std::vector<vk::raii::DescriptorSet> DescriptorSets = Device.allocateDescriptorSets(AllocateInfo);

    for (uint32_t FrameIndex = 0; FrameIndex < FramesInFlight; FrameIndex++)
    {
        FrameResources& Frame = Frames[FrameIndex];

        Frame.DescriptorSet = std::move(DescriptorSets[FrameIndex]);

        const vk::DescriptorBufferInfo BufferInfo(*Frame.UniformBuffer, 0, sizeof(DirectionalShadowUniformData));

        const vk::WriteDescriptorSet Write(*Frame.DescriptorSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &BufferInfo);

        Device.updateDescriptorSets(Write, {});
    }
}

void DirectionalShadowDescriptorSet::UpdateUniform(uint32_t FrameIndex, const DirectionalShadowUniformData& Data)
{
    FrameResources& Frame = Frames.at(FrameIndex);

    std::memcpy(Frame.MappedMemory, &Data, sizeof(DirectionalShadowUniformData));
}

void DirectionalShadowDescriptorSet::UpdateImage(uint32_t FrameIndex, vk::ImageView ImageView)
{
    FrameResources& Frame = Frames.at(FrameIndex);

    const vk::DescriptorImageInfo ImageInfo(*Sampler, ImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

    const vk::WriteDescriptorSet Write(*Frame.DescriptorSet, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &ImageInfo);

    Device.updateDescriptorSets(Write, {});
}