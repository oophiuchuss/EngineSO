module;

#include <cstdint>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

module DirectionalShadowMap;

import VulkanUtils;

DirectionalShadowMap::DirectionalShadowMap(
    const vk::raii::Device& InDevice,
    const vk::raii::PhysicalDevice& InPhysicalDevice,
    uint32_t InFramesInFlight,
    uint32_t InResolution) :
    Device(InDevice),
    PhysicalDevice(InPhysicalDevice),
    FramesInFlight(InFramesInFlight)
{
    if (FramesInFlight == 0)
    {
        throw std::invalid_argument("DirectionalShadowMap: frames in flight must be greater than zero");
    }

    Recreate(InResolution);
}

void DirectionalShadowMap::Recreate(uint32_t InResolution)
{
    if (InResolution == 0)
    {
        throw std::invalid_argument("DirectionalShadowMap: resolution must be greater than zero");
    }

    Images.clear();

    Resolution = InResolution;

    Images.reserve(FramesInFlight);

    for (uint32_t FrameIndex = 0; FrameIndex < FramesInFlight; FrameIndex++)
    {
        Images.push_back(CreateShadowImage());
    }
}

DirectionalShadowMap::ShadowImage DirectionalShadowMap::CreateShadowImage()
{
    vk::ImageCreateInfo ImageInfo;

    ImageInfo.setImageType(vk::ImageType::e2D)
        .setFormat(Format)
        .setExtent(vk::Extent3D(Resolution, Resolution, 1))
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);

    ShadowImage Result;

    Result.Image = Device.createImage(ImageInfo);

    const vk::MemoryRequirements MemoryRequirements = Result.Image.getMemoryRequirements();

    const uint32_t MemoryTypeIndex = VulkanUtils::FindMemoryType(
        PhysicalDevice, MemoryRequirements.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::MemoryAllocateInfo AllocationInfo;

    AllocationInfo.setAllocationSize(MemoryRequirements.size)
        .setMemoryTypeIndex(MemoryTypeIndex);

    Result.Memory = Device.allocateMemory(AllocationInfo);

    Result.Image.bindMemory(*Result.Memory, 0);

    vk::ImageViewCreateInfo ViewInfo;

    ViewInfo.setImage(*Result.Image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(Format)
        .setSubresourceRange(vk::ImageSubresourceRange(
                vk::ImageAspectFlagBits::eDepth,
                0, 1, 0, 1));

    Result.View = Device.createImageView(ViewInfo);

    return Result;
}

vk::Image DirectionalShadowMap::GetImage(uint32_t FrameIndex) const
{
    return *Images.at(FrameIndex).Image;
}

vk::ImageView DirectionalShadowMap::GetImageView(uint32_t FrameIndex) const
{
    return *Images.at(FrameIndex).View;
}