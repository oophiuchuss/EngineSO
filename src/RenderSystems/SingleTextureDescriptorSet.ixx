module;

#include <vulkan/vulkan_raii.hpp>
#include <string>

export module SingleTextureDescriptorSet;

import Rendergraph;

// Potentialy make a parent for descriptor sets, like GBufferDescriptorSet and SingleTextureDescriptorSet
export class SingleTextureDescriptorSet
{
public:
    SingleTextureDescriptorSet(
        const vk::raii::Device& InDevice,
        const std::string& InResourceName);

    // Must be called after Rendergraph::Compile() – the resource's view is then valid
    void Initialize(Rendergraph& Graph);

    void ResetDescriptorSet();

    vk::DescriptorSet GetDescriptorSet() const { return *DescriptorSet; }
    vk::DescriptorSetLayout GetDescriptorSetLayout() const { return *DescriptorLayout; }

private:
    const vk::raii::Device& Device;
    std::string ResourceName;

    vk::raii::Sampler Sampler = nullptr;
    vk::raii::DescriptorSetLayout DescriptorLayout = nullptr;
    vk::raii::DescriptorPool DescriptorPool = nullptr;
    vk::raii::DescriptorSet DescriptorSet = nullptr;
};