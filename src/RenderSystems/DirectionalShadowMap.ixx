module;

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module DirectionalShadowMap;

export class DirectionalShadowMap
{
public:
    static constexpr vk::Format Format = vk::Format::eD32Sfloat;

    DirectionalShadowMap(
        const vk::raii::Device& InDevice,
        const vk::raii::PhysicalDevice& InPhysicalDevice,
        uint32_t InFramesInFlight,
        uint32_t InResolution);

    void Recreate(uint32_t InResolution);

    vk::Image GetImage(uint32_t FrameIndex) const;

    vk::ImageView GetImageView(uint32_t FrameIndex) const;

    uint32_t GetResolution() const { return Resolution; }

private:
    struct ShadowImage
    {
        // Reverse destruction order:
        // View -> Image -> Memory.
        vk::raii::DeviceMemory Memory = nullptr;
        vk::raii::Image Image = nullptr;
        vk::raii::ImageView View = nullptr;
    };

    ShadowImage CreateShadowImage();

    const vk::raii::Device& Device;
    const vk::raii::PhysicalDevice& PhysicalDevice;

    uint32_t FramesInFlight = 0;
    uint32_t Resolution = 0;

    std::vector<ShadowImage> Images;
};