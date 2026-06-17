module;

#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <stdexcept>

export module DescriptorHeap;

import SamplerDesc;

export class DescriptorHeap
{
public:
    DescriptorHeap(const vk::raii::Device& InDevice, uint32_t InMaxTextures);
    ~DescriptorHeap() = default;

    int AllocateSlot();
    void FreeSlot(int Slot);

    // Write a texture image view into a slot with a specific sampler
    void WriteSlot(int Slot, vk::ImageView View, const SamplerDesc& Desc = PresetSamplerDesc::SamplerLinearRepeat);

    const vk::raii::DescriptorSet& GetDescriptorSet() const { return DescriptorSet; }
    const vk::raii::DescriptorSetLayout& GetDescriptorSetLayout() const { return Layout; }

    static vk::Filter ToVkFilter(FilterMode Mode);
    static vk::SamplerMipmapMode ToVkMipmapMode(MipmapMode Mode);
    static vk::SamplerAddressMode ToVkAddressMode(WrapMode Mode);

private:
    void CreateDescriptorLayout();
    void CreateDescriptorPool();
    void CreateDescriptorSet();

    vk::raii::Sampler& GetOrCreateSampler(const SamplerDesc& Desc);

    const vk::raii::Device& Device;
    uint32_t MaxTextures;

    vk::raii::Sampler Sampler = nullptr;
    vk::raii::DescriptorSetLayout Layout = nullptr;
    vk::raii::DescriptorPool Pool = nullptr;
    vk::raii::DescriptorSet DescriptorSet = nullptr;

    std::vector<int> FreeSlots; // internal index allocator

    // Sampler cache 
    std::vector<std::pair<SamplerDesc, vk::raii::Sampler>> SamplerCache;
};