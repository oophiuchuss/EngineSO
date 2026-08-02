module;

#include <cstdint>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

export module TemporalAADescriptorSet;

export class TemporalAADescriptorSet
{
public:
    TemporalAADescriptorSet(const vk::raii::Device& InDevice, uint32_t InFramesInFlight);

    void Initialize();

    void Update(
        uint32_t FrameIndex,
        vk::ImageView CurrentColor,
        vk::ImageView CurrentDepth,
        vk::ImageView Velocity,
        vk::ImageView HistoryColor,
        vk::ImageView HistoryDepth);

    void ResetDescriptorSet();

    const vk::raii::DescriptorSet& GetDescriptorSet(uint32_t FrameIndex) const
    {
        return DescriptorSets.at(FrameIndex);
    }

    const vk::raii::DescriptorSetLayout& GetDescriptorSetLayout() const
    {
        return DescriptorLayout;
    }

private:
    const vk::raii::Device& Device;
    uint32_t FramesInFlight = 0;

    vk::raii::Sampler LinearSampler = nullptr;
    vk::raii::Sampler NearestSampler = nullptr;
    vk::raii::DescriptorSetLayout DescriptorLayout = nullptr;
    vk::raii::DescriptorPool DescriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> DescriptorSets;
};