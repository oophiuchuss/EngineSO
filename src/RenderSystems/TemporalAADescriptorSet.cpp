module;

#include <array>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

module TemporalAADescriptorSet;

TemporalAADescriptorSet::TemporalAADescriptorSet(
    const vk::raii::Device& InDevice,
    uint32_t InFramesInFlight):
    Device(InDevice),
    FramesInFlight(InFramesInFlight)
{
}

void TemporalAADescriptorSet::Initialize()
{
    vk::SamplerCreateInfo LinearInfo;
    LinearInfo.setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeW(vk::SamplerAddressMode::eClampToEdge);

    LinearSampler = Device.createSampler(LinearInfo);

    vk::SamplerCreateInfo NearestInfo;
    NearestInfo.setMagFilter(vk::Filter::eNearest)
        .setMinFilter(vk::Filter::eNearest)
        .setMipmapMode(vk::SamplerMipmapMode::eNearest)
        .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeW(vk::SamplerAddressMode::eClampToEdge);

    NearestSampler = Device.createSampler(NearestInfo);

    std::array<vk::DescriptorSetLayoutBinding, 5> Bindings;

    for (uint32_t BindingIndex = 0; BindingIndex < Bindings.size(); ++BindingIndex)
    {
        Bindings[BindingIndex] = vk::DescriptorSetLayoutBinding(
            BindingIndex,
            vk::DescriptorType::eCombinedImageSampler,
            1,
            vk::ShaderStageFlagBits::eFragment);
    }

    vk::DescriptorSetLayoutCreateInfo LayoutInfo;
    LayoutInfo.setBindings(Bindings);

    DescriptorLayout = Device.createDescriptorSetLayout(LayoutInfo);

    const vk::DescriptorPoolSize PoolSize(
        vk::DescriptorType::eCombinedImageSampler,
        FramesInFlight * static_cast<uint32_t>(Bindings.size()));

    vk::DescriptorPoolCreateInfo PoolInfo;
    PoolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(FramesInFlight)
        .setPoolSizes(PoolSize);

    DescriptorPool = Device.createDescriptorPool(PoolInfo);

    const std::vector<vk::DescriptorSetLayout> Layouts(FramesInFlight, *DescriptorLayout);

    vk::DescriptorSetAllocateInfo AllocateInfo;
    AllocateInfo.setDescriptorPool(*DescriptorPool)
        .setSetLayouts(Layouts);

    DescriptorSets = Device.allocateDescriptorSets(AllocateInfo);
}

void TemporalAADescriptorSet::Update(
    uint32_t FrameIndex,
    vk::ImageView CurrentColor,
    vk::ImageView CurrentDepth,
    vk::ImageView Velocity,
    vk::ImageView HistoryColor,
    vk::ImageView HistoryDepth)
{
    if (FrameIndex >= DescriptorSets.size())
    {
        throw std::out_of_range("TemporalAADescriptorSet frame index is out of range");
    }

    const std::array<vk::DescriptorImageInfo, 5> ImageInfos =
    {
        vk::DescriptorImageInfo(
            *LinearSampler, CurrentColor,
            vk::ImageLayout::eShaderReadOnlyOptimal),

        vk::DescriptorImageInfo(
            *NearestSampler, CurrentDepth,
            vk::ImageLayout::eShaderReadOnlyOptimal),

        vk::DescriptorImageInfo(
            *NearestSampler, Velocity,
            vk::ImageLayout::eShaderReadOnlyOptimal),

        vk::DescriptorImageInfo(
            *LinearSampler, HistoryColor,
            vk::ImageLayout::eShaderReadOnlyOptimal),

        vk::DescriptorImageInfo(
            *NearestSampler, HistoryDepth,
            vk::ImageLayout::eShaderReadOnlyOptimal)
    };

    std::array<vk::WriteDescriptorSet, 5> Writes;

    for (uint32_t BindingIndex = 0; BindingIndex < Writes.size(); BindingIndex++)
    {
        Writes[BindingIndex] = vk::WriteDescriptorSet(
            *DescriptorSets[FrameIndex],
            BindingIndex,
            0,
            1,
            vk::DescriptorType::eCombinedImageSampler,
            &ImageInfos[BindingIndex]);
    }

    Device.updateDescriptorSets(Writes, {});
}

void TemporalAADescriptorSet::ResetDescriptorSet()
{
    DescriptorSets.clear();
    DescriptorPool = nullptr;
    DescriptorLayout = nullptr;
    NearestSampler = nullptr;
    LinearSampler = nullptr;
}