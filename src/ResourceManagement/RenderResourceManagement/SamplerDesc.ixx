module;

export module SamplerDesc;

export enum class FilterMode { Nearest, Linear };
export enum class WrapMode { Repeat, Clamp, Mirror };
export enum class MipmapMode { Nearest, Linear };

export struct SamplerDesc
{
    FilterMode MagFilter = FilterMode::Linear;
    FilterMode MinFilter = FilterMode::Linear;
    MipmapMode MipMode = MipmapMode::Linear;
    WrapMode AddressU = WrapMode::Repeat;
    WrapMode AddressV = WrapMode::Repeat;
    WrapMode AddressW = WrapMode::Repeat;
    float MipLodBias = 0.0f;
    bool Anisotropy = false;
    float MaxAniso = 1.0f;
    float MinLod = 0.0f;
    float MaxLod = 1000.0f; // unclamped — ready for mipmaps

    bool operator==(const SamplerDesc&) const = default;
};

export namespace PresetSamplerDesc
{
    constexpr SamplerDesc SamplerLinearRepeat = {
        FilterMode::Linear, FilterMode::Linear, MipmapMode::Linear,
        WrapMode::Repeat, WrapMode::Repeat, WrapMode::Repeat,
        0.0f, false, 1.0f, 0.0f, 1000.0f
    };

    constexpr SamplerDesc SamplerLinearClamp = {
        FilterMode::Linear, FilterMode::Linear, MipmapMode::Linear,
        WrapMode::Clamp, WrapMode::Clamp, WrapMode::Clamp,
        0.0f, false, 1.0f, 0.0f, 1000.0f
    };

    constexpr SamplerDesc SamplerNearestRepeat = {
        FilterMode::Nearest, FilterMode::Nearest, MipmapMode::Nearest,
        WrapMode::Repeat, WrapMode::Repeat, WrapMode::Repeat,
        0.0f, false, 1.0f, 0.0f, 1000.0f
    };
}