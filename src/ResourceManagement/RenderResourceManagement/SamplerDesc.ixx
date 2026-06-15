module;

#include <vulkan/vulkan.hpp>

export module SamplerDesc;

// Describes a complete sampler state
export struct SamplerDesc
{
    vk::Filter             MagFilter = vk::Filter::eLinear;
    vk::Filter             MinFilter = vk::Filter::eLinear;
    vk::SamplerMipmapMode  MipmapMode = vk::SamplerMipmapMode::eLinear;
    vk::SamplerAddressMode AddressModeU = vk::SamplerAddressMode::eRepeat;
    vk::SamplerAddressMode AddressModeV = vk::SamplerAddressMode::eRepeat;
    vk::SamplerAddressMode AddressModeW = vk::SamplerAddressMode::eRepeat;
    float                  MipLodBias = 0.0f;
    bool                   AnisotropyEnable = false;
    float                  MaxAnisotropy = 1.0f;
    float                  MinLod = 0.0f;
    float                  MaxLod = 0.0f;

    bool operator==(const SamplerDesc& Other) const = default;
};

export namespace PresetSamplerDesc
{
    constexpr SamplerDesc SamplerLinearRepeat = {
       vk::Filter::eLinear, vk::Filter::eLinear,
       vk::SamplerMipmapMode::eLinear,
       vk::SamplerAddressMode::eRepeat,
       vk::SamplerAddressMode::eRepeat,
       vk::SamplerAddressMode::eRepeat
    };

    constexpr SamplerDesc SamplerLinearClamp = {
        vk::Filter::eLinear, vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge
    };

    constexpr SamplerDesc SamplerNearestRepeat = {
        vk::Filter::eNearest, vk::Filter::eNearest,
        vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat
    };
}

