module;

#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>
export module DescriptorHeap;

import SamplerDesc;
import VulkanUploader;

export enum class TextureDescriptorType
{
    Texture2D,
    Cubemap
};

export struct TextureDescriptorAllocation
{
    TextureDescriptorType Type = TextureDescriptorType::Texture2D;
    int Slot = -1;

    bool IsValid() const
    {
        return Slot >= 0;
    }
};

export class DescriptorHeap
{
public:
    DescriptorHeap(
        const vk::raii::PhysicalDevice& InPhysicalDevice, 
        const vk::raii::Device& InDevice, 
        uint32_t InMaxTexture2DCount, 
        uint32_t InMaxCubemapCount, 
        VulkanUploader& InUploader);

    ~DescriptorHeap() = default;

    TextureDescriptorAllocation AllocateSlot(TextureDescriptorType Type);
    void FreeSlot(TextureDescriptorAllocation Allocation);

    // Write a texture image view into a slot with a specific sampler
    void WriteSlot(TextureDescriptorAllocation Allocation, vk::ImageView View, const SamplerDesc& Desc = PresetSamplerDesc::SamplerLinearRepeat);

    const vk::raii::DescriptorSet& GetDescriptorSet() const { return DescriptorSet; }
    const vk::raii::DescriptorSetLayout& GetDescriptorSetLayout() const { return Layout; }

    static vk::Filter ToVkFilter(FilterMode Mode);
    static vk::SamplerMipmapMode ToVkMipmapMode(MipmapMode Mode);
    static vk::SamplerAddressMode ToVkAddressMode(WrapMode Mode);

private:
    void CreateDescriptorLayout();
    void CreateDescriptorPool();
    void CreateDescriptorSet();
    void CreateDefaultTextures();

    uint32_t GetBinding(TextureDescriptorType Type) const;
    uint32_t GetCapacity(TextureDescriptorType Type) const;
    std::vector<int>& GetFreeSlots(TextureDescriptorType Type);

    vk::raii::Sampler& GetOrCreateSampler(const SamplerDesc& Desc);

    struct DefaultTexture
    {
        vk::raii::DeviceMemory Memory = nullptr;
        vk::raii::Image Image = nullptr;
        vk::raii::ImageView View = nullptr;
    };

    static constexpr uint32_t Texture2DBinding = 0;
    static constexpr uint32_t CubemapBinding = 1;

    const vk::raii::PhysicalDevice& PhysicalDevice;
    const vk::raii::Device& Device;

    uint32_t MaxTexture2DCount;
    uint32_t MaxCubemapCount;

    vk::raii::DescriptorSetLayout Layout = nullptr;
    vk::raii::DescriptorPool Pool = nullptr;
    vk::raii::DescriptorSet DescriptorSet = nullptr;

    VulkanUploader& Uploader;

    std::array<DefaultTexture, 3> DefaultTexture2Ds;
    DefaultTexture DefaultCubemap;

    std::vector<int> FreeTexture2DSlots;
    std::vector<int> FreeCubemapSlots;

    std::vector<std::pair<SamplerDesc, vk::raii::Sampler>> SamplerCache;
};
