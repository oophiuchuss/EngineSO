module;

#include <array>
#include <stdexcept>
#include <vulkan/vulkan_raii.hpp>

module TemporalAAHistory;

import VulkanUtils;

TemporalAAHistory::TemporalAAHistory(
    const vk::raii::Device& InDevice,
    const vk::raii::PhysicalDevice& InPhysicalDevice,
    vk::Extent2D InExtent) :
    Device(InDevice),
    PhysicalDevice(InPhysicalDevice)
{
    Recreate(InExtent);
}

void TemporalAAHistory::Recreate(vk::Extent2D InExtent)
{
    if (InExtent.width == 0 || InExtent.height == 0)
    {
        throw std::invalid_argument("TemporalAAHistory cannot use a zero-sized extent");
    }

    ReleaseImages();

    Extent = InExtent;

    for (HistorySet& Set : Sets)
    {
        Set.Color = CreateHistoryImage(ColorFormat);
        Set.Depth = CreateHistoryImage(DepthFormat);
    }

    bNeedsInitialization = true;
}

TemporalAAHistory::HistoryImage TemporalAAHistory::CreateHistoryImage(vk::Format Format)
{
    const vk::ImageUsageFlags Usage =
        vk::ImageUsageFlagBits::eColorAttachment |
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eTransferDst;

    vk::ImageCreateInfo ImageInfo;
    ImageInfo.setImageType(vk::ImageType::e2D)
        .setFormat(Format)
        .setExtent(vk::Extent3D(Extent.width, Extent.height, 1))
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(Usage)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);

    HistoryImage Result;
    Result.Image = Device.createImage(ImageInfo);

    const vk::MemoryRequirements MemoryRequirements = Result.Image.getMemoryRequirements();

    const uint32_t MemoryTypeIndex = VulkanUtils::FindMemoryType(
        PhysicalDevice,
        MemoryRequirements.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::MemoryAllocateInfo AllocateInfo;
    AllocateInfo.setAllocationSize(MemoryRequirements.size)
        .setMemoryTypeIndex(MemoryTypeIndex);

    Result.Memory = Device.allocateMemory(AllocateInfo);
    Result.Image.bindMemory(*Result.Memory, 0);

    vk::ImageViewCreateInfo ViewInfo;
    ViewInfo.setImage(*Result.Image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(Format)
        .setSubresourceRange(vk::ImageSubresourceRange(
                vk::ImageAspectFlagBits::eColor,
                0, 1, 0, 1));

    Result.View = Device.createImageView(ViewInfo);

    return Result;
}

void TemporalAAHistory::RecordInitialization(vk::raii::CommandBuffer& Cmd)
{
    if (!bNeedsInitialization)
    {
        return;
    }

    const vk::ClearColorValue ColorClear(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f });

    const vk::ClearColorValue DepthClear(std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f });

    for (HistorySet& Set : Sets)
    {
        ClearHistoryImage(Cmd, *Set.Color.Image, ColorClear);
        ClearHistoryImage(Cmd, *Set.Depth.Image, DepthClear);
    }
}

void TemporalAAHistory::ClearHistoryImage(vk::raii::CommandBuffer& Cmd, vk::Image Image, const vk::ClearColorValue& ClearValue)
{
    VulkanUtils::TransitionImageLayout(
        Cmd,
        Image,
        vk::ImageAspectFlagBits::eColor,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal);

    const std::array<vk::ImageSubresourceRange, 1> Ranges =
    {
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
    };

    Cmd.clearColorImage(
        Image,
        vk::ImageLayout::eTransferDstOptimal,
        ClearValue,
        Ranges);

    VulkanUtils::TransitionImageLayout(
        Cmd,
        Image,
        vk::ImageAspectFlagBits::eColor,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal);
}

void TemporalAAHistory::CommitInitialization()
{
    bNeedsInitialization = false;
}

const TemporalAAHistory::HistorySet& TemporalAAHistory::GetReadSet(uint64_t TemporalFrameIndex) const
{
    return Sets[TemporalFrameIndex % 2];
}

const TemporalAAHistory::HistorySet& TemporalAAHistory::GetWriteSet(uint64_t TemporalFrameIndex) const
{
    return Sets[1 - (TemporalFrameIndex % 2)];
}

vk::Image TemporalAAHistory::GetReadColorImage(uint64_t TemporalFrameIndex) const
{
    return *GetReadSet(TemporalFrameIndex).Color.Image;
}

vk::ImageView TemporalAAHistory::GetReadColorView(uint64_t TemporalFrameIndex) const
{
    return *GetReadSet(TemporalFrameIndex).Color.View;
}

vk::Image TemporalAAHistory::GetWriteColorImage(uint64_t TemporalFrameIndex) const
{
    return *GetWriteSet(TemporalFrameIndex).Color.Image;
}

vk::ImageView TemporalAAHistory::GetWriteColorView(
    uint64_t TemporalFrameIndex) const
{
    return *GetWriteSet(TemporalFrameIndex).Color.View;
}

vk::Image TemporalAAHistory::GetReadDepthImage(
    uint64_t TemporalFrameIndex) const
{
    return *GetReadSet(TemporalFrameIndex).Depth.Image;
}

vk::ImageView TemporalAAHistory::GetReadDepthView(
    uint64_t TemporalFrameIndex) const
{
    return *GetReadSet(TemporalFrameIndex).Depth.View;
}

vk::Image TemporalAAHistory::GetWriteDepthImage(
    uint64_t TemporalFrameIndex) const
{
    return *GetWriteSet(TemporalFrameIndex).Depth.Image;
}

vk::ImageView TemporalAAHistory::GetWriteDepthView(
    uint64_t TemporalFrameIndex) const
{
    return *GetWriteSet(TemporalFrameIndex).Depth.View;
}

void TemporalAAHistory::ReleaseImages()
{
    for (HistorySet& Set : Sets)
    {
        Set.Color.View = nullptr;
        Set.Color.Image = nullptr;
        Set.Color.Memory = nullptr;

        Set.Depth.View = nullptr;
        Set.Depth.Image = nullptr;
        Set.Depth.Memory = nullptr;
    }
}