module;

#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

module SingleTextureDescriptorSet;

SingleTextureDescriptorSet::SingleTextureDescriptorSet(
    const vk::raii::Device& InDevice,
    uint32_t InFramesInFlight) :
    Device(InDevice),
    FramesInFlight(InFramesInFlight)
{
}

void SingleTextureDescriptorSet::Initialize()
{
    vk::SamplerCreateInfo SamplerInfo;
    SamplerInfo.setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeW(vk::SamplerAddressMode::eClampToEdge);

    Sampler = Device.createSampler(SamplerInfo);

    const vk::DescriptorSetLayoutBinding Binding(
        0,
        vk::DescriptorType::eCombinedImageSampler,
        1,
        vk::ShaderStageFlagBits::eFragment);

    vk::DescriptorSetLayoutCreateInfo LayoutInfo;
    LayoutInfo.setBindings(Binding);

    DescriptorLayout = Device.createDescriptorSetLayout(LayoutInfo);

    const vk::DescriptorPoolSize PoolSize(vk::DescriptorType::eCombinedImageSampler, FramesInFlight);

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

void SingleTextureDescriptorSet::Update(uint32_t FrameIndex, vk::ImageView ImageView)
{
    if (FrameIndex >= DescriptorSets.size())
    {
        throw std::out_of_range("SingleTextureDescriptorSet frame index is out of range");
    }

    const vk::DescriptorImageInfo ImageInfo(
        *Sampler,
        ImageView,
        vk::ImageLayout::eShaderReadOnlyOptimal);

    const vk::WriteDescriptorSet Write(
        *DescriptorSets[FrameIndex],
        0, 0, 1,
        vk::DescriptorType::eCombinedImageSampler,
        &ImageInfo);

    Device.updateDescriptorSets(Write, {});
}

void SingleTextureDescriptorSet::ResetDescriptorSet()
{
    DescriptorSets.clear();
    DescriptorPool = nullptr;
    DescriptorLayout = nullptr;
    Sampler = nullptr;
}