module;

#include <vulkan/vulkan_raii.hpp>

module GBufferDescriptorSet;

import Rendergraph;

GBufferDescriptorSet::GBufferDescriptorSet(
    const vk::raii::Device& InDevice,
    const std::string& InAlbedoName,
    const std::string& InNormalName,
    const std::string& InMetalRoughName,
    const std::string& InEmissiveName,
    const std::string& InDepthName):
    Device(InDevice),
    AlbedoName(InAlbedoName),
    NormalName(InNormalName),
    MetalRoughName(InMetalRoughName),
    EmissiveName(InEmissiveName),
    DepthName(InDepthName)
{
}

void GBufferDescriptorSet::Initialize(Rendergraph& Graph)
{
    Resource* Albedo = Graph.GetResource(AlbedoName);
    Resource* Normal = Graph.GetResource(NormalName);
    Resource* MetalRough = Graph.GetResource(MetalRoughName);
    Resource* Emissive = Graph.GetResource(EmissiveName);
    Resource* Depth = Graph.GetResource(DepthName);

    if (!Albedo || !Normal || !MetalRough || !Emissive || !Depth)
        throw std::runtime_error("GBufferDescriptorSet: resource missing");

    // Sampler – nearest, clamp
    vk::SamplerCreateInfo SamplerInfo;
    SamplerInfo.setMagFilter(vk::Filter::eNearest).setMinFilter(vk::Filter::eNearest)
        .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
        .setMipmapMode(vk::SamplerMipmapMode::eNearest)
        .setMinLod(0.0f).setMaxLod(0.0f);
    Sampler = Device.createSampler(SamplerInfo);

    // Layout
    std::array<vk::DescriptorSetLayoutBinding, 5> Bindings = { {
        { 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },
        { 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },
        { 2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },
        { 3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },
        { 4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },
    } };
    vk::DescriptorSetLayoutCreateInfo LayoutInfo({}, Bindings);
    Layout = Device.createDescriptorSetLayout(LayoutInfo);

    // Pool
    vk::DescriptorPoolSize PoolSize(vk::DescriptorType::eCombinedImageSampler, 5);
    vk::DescriptorPoolCreateInfo PoolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, PoolSize);

    Pool = Device.createDescriptorPool(PoolInfo);

    // Set
    vk::DescriptorSetAllocateInfo AllocInfo(*Pool, 1, &*Layout);
    DescriptorSet = std::move(Device.allocateDescriptorSets(AllocInfo).front());

    // Write
    std::array<vk::DescriptorImageInfo, 5> ImageInfos = { {
        { *Sampler, Albedo->View, vk::ImageLayout::eShaderReadOnlyOptimal },
        { *Sampler, Normal->View, vk::ImageLayout::eShaderReadOnlyOptimal },
        { *Sampler, MetalRough->View, vk::ImageLayout::eShaderReadOnlyOptimal },
        { *Sampler, Emissive->View, vk::ImageLayout::eShaderReadOnlyOptimal },
        { *Sampler, Depth->View, vk::ImageLayout::eShaderReadOnlyOptimal },
    } };

    std::array<vk::WriteDescriptorSet, 5> Writes;
    for (int i = 0; i < 5; ++i)
        Writes[i].setDstSet(*DescriptorSet).setDstBinding(i)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setPImageInfo(&ImageInfos[i]);
    Device.updateDescriptorSets(Writes, {});
}