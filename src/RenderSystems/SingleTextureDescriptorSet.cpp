module;

#include <vulkan/vulkan_raii.hpp>

module SingleTextureDescriptorSet;

SingleTextureDescriptorSet::SingleTextureDescriptorSet(
    const vk::raii::Device& InDevice,
    const std::string& InResourceName)
    : Device(InDevice)
    , ResourceName(InResourceName)
{
}

void SingleTextureDescriptorSet::Initialize(Rendergraph& Graph)
{
    // Sampler – linear, clamp to edge (safe for full‑screen passes)
    vk::SamplerCreateInfo SamplerInfo;
    SamplerInfo.setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
    Sampler = Device.createSampler(SamplerInfo);

    // Descriptor set layout – single combined image sampler at binding 0
    vk::DescriptorSetLayoutBinding Binding(
        0,                                          // binding
        vk::DescriptorType::eCombinedImageSampler,
        1,
        vk::ShaderStageFlagBits::eFragment);
    vk::DescriptorSetLayoutCreateInfo LayoutInfo({}, 1, &Binding);
    DescriptorLayout = Device.createDescriptorSetLayout(LayoutInfo);

    vk::DescriptorPoolSize PoolSize(vk::DescriptorType::eCombinedImageSampler, 1);
    vk::DescriptorPoolCreateInfo PoolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        1, PoolSize);
    DescriptorPool = Device.createDescriptorPool(PoolInfo);

    // Allocate descriptor set
    vk::DescriptorSetAllocateInfo AllocInfo(*DescriptorPool, 1, &*DescriptorLayout);
    DescriptorSet = std::move(Device.allocateDescriptorSets(AllocInfo).front());

    // Write the descriptor with the resource's current image view
    vk::ImageView View = Graph.GetResourceView(ResourceName);
    vk::DescriptorImageInfo ImageInfo(*Sampler, View, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::WriteDescriptorSet Write(
        *DescriptorSet,
        0, 0, 1,
        vk::DescriptorType::eCombinedImageSampler,
        &ImageInfo);
    Device.updateDescriptorSets(Write, {});
}

void SingleTextureDescriptorSet::ResetDescriptorSet()
{
    DescriptorSet = nullptr;
    DescriptorPool = nullptr;
    DescriptorLayout = nullptr;
    Sampler = nullptr;
}
