module;

#include <vulkan/vulkan_raii.hpp>
#include <string>
#include <array>
#include <stdexcept>

export module GBufferDescriptorSet;

import Rendergraph;

export class GBufferDescriptorSet
{
public:
    GBufferDescriptorSet(
        const vk::raii::Device& Device,
        const std::string& AlbedoName,
        const std::string& NormalName,
        const std::string& MetalRoughName,
        const std::string& EmissiveName,
        const std::string& DepthName);

    // Must be called once after Rendergraph::Compile()
    void Initialize(Rendergraph& Graph);
   
    void ResetDescriptorSet();

    vk::DescriptorSet GetDescriptorSet() const { return *DescriptorSet; }
    vk::DescriptorSetLayout GetDescriptorSetLayout() const { return *Layout; }

private:
    const vk::raii::Device& Device;
    std::string AlbedoName;
    std::string NormalName;
    std::string MetalRoughName;
    std::string EmissiveName;
    std::string DepthName;

    vk::raii::Sampler Sampler = nullptr;
    vk::raii::DescriptorSetLayout Layout = nullptr;
    vk::raii::DescriptorPool Pool = nullptr;
    vk::raii::DescriptorSet DescriptorSet = nullptr;
};