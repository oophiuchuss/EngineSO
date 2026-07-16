module;

#include <string>
#include <vector>

export module TextureData;

import ResourceBase;

import SamplerDesc;

export enum class TextureColorSpace
{
	Linear,	// Normal maps, metallic, roughness, AO - data textures
	SRGB,	// Albedo, emmisive - color textures viewed by human eye
};

export enum class TextureMipFilter
{
	Standard,	// Standard mipmap generation (box filter)
	NormalMap,	// Normal map mipmap generation (preserves normals)
};

export struct TextureInfo
{
	uint32_t Width = 0;
	uint32_t Height = 0;
	uint32_t Channels = 0;
	TextureColorSpace ColorSpace = TextureColorSpace::SRGB;
	SamplerDesc Sampler = PresetSamplerDesc::SamplerLinearRepeat;
	TextureMipFilter MipFilter = TextureMipFilter::Standard;
};

export class TextureData : public ResourceBase
{
public:
	explicit TextureData(
		const std::string& ID,
		TextureColorSpace InColorSpace = TextureColorSpace::SRGB,
		SamplerDesc InSampler = PresetSamplerDesc::SamplerLinearRepeat,
		TextureMipFilter InMipFilter = TextureMipFilter::Standard):
		ResourceBase(ID)
	{
		Info.ColorSpace = InColorSpace;
		Info.Sampler = InSampler;
		Info.MipFilter = InMipFilter;
	}

	// Accessors
	inline const std::vector<uint8_t>& GetPixels()   const { return Pixels; }
	inline const TextureInfo& GetInfo() const { return Info; }
	inline uint32_t GetWidth() const { return Info.Width; }
	inline uint32_t GetHeight() const { return Info.Height; }
	inline uint32_t GetChannels() const { return Info.Channels; }
	inline TextureColorSpace GetColorSpace() const { return Info.ColorSpace; }
	inline TextureMipFilter GetMipFilter() const { return Info.MipFilter; }
	const SamplerDesc& GetSamplerDesc() const { return Info.Sampler; }

	// Extension empty — ID includes extension e.g. "albedo.png"
	static std::string_view AssetFolder() { return "textures"; }
	static std::string_view FileExtension() { return ""; }

protected:
	bool LoadResource(const std::string& FilePath) override;
	bool LoadResourceFromMemory(const std::vector<uint8_t>& Data) override;
	void UnloadResource() override;

private:
	bool DecodePixels(const uint8_t* Data, int DataSize);

	std::vector<uint8_t> Pixels;
	TextureInfo Info;
};