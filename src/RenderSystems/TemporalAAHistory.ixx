module;

#include <array>
#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

export module TemporalAAHistory;

export class TemporalAAHistory
{
public:
    static constexpr vk::Format ColorFormat = vk::Format::eR16G16B16A16Sfloat;

    // This is color data containing device depth, not a depth attachment.
    static constexpr vk::Format DepthFormat = vk::Format::eR32Sfloat;

    TemporalAAHistory(
        const vk::raii::Device& InDevice, 
        const vk::raii::PhysicalDevice& InPhysicalDevice,
        vk::Extent2D InExtent);

    void Recreate(vk::Extent2D InExtent);

    // Records initial clears and layout transitions. It deliberately does not
    // mark initialization complete until the command buffer is submitted.
    void RecordInitialization(vk::raii::CommandBuffer& Cmd);

    // Called only after successful queue submission.
    void CommitInitialization();

    bool NeedsInitialization() const
    {
        return bNeedsInitialization;
    }

    vk::Image GetReadColorImage(uint64_t TemporalFrameIndex) const;
    vk::ImageView GetReadColorView(uint64_t TemporalFrameIndex) const;

    vk::Image GetWriteColorImage(uint64_t TemporalFrameIndex) const;
    vk::ImageView GetWriteColorView(uint64_t TemporalFrameIndex) const;

    vk::Image GetReadDepthImage(uint64_t TemporalFrameIndex) const;
    vk::ImageView GetReadDepthView(uint64_t TemporalFrameIndex) const;

    vk::Image GetWriteDepthImage(uint64_t TemporalFrameIndex) const;
    vk::ImageView GetWriteDepthView(uint64_t TemporalFrameIndex) const;

private:
    struct HistoryImage
    {
        // Declaration order ensures destruction happens as:
        // View -> Image -> Memory.
        vk::raii::DeviceMemory Memory = nullptr;
        vk::raii::Image Image = nullptr;
        vk::raii::ImageView View = nullptr;
    };

    struct HistorySet
    {
        HistoryImage Color;
        HistoryImage Depth;
    };

    HistoryImage CreateHistoryImage(vk::Format Format);

    const HistorySet& GetReadSet(uint64_t TemporalFrameIndex) const;
    const HistorySet& GetWriteSet(uint64_t TemporalFrameIndex) const;

    void ClearHistoryImage(vk::raii::CommandBuffer& Cmd, vk::Image Image, const vk::ClearColorValue& ClearValue);

    void ReleaseImages();

    const vk::raii::Device& Device;
    const vk::raii::PhysicalDevice& PhysicalDevice;

    vk::Extent2D Extent{};
    std::array<HistorySet, 2> Sets;

    bool bNeedsInitialization = true;
};