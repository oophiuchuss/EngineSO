module;

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <string>
#include <vector>
#include <cstddef>
#include <optional>

export module TextureData;

import ResourceBase;

import SamplerDesc;

export enum class TextureColorSpace
{
	Linear,	// Normal maps, metallic, roughness, AO - data textures
	SRGB,	// Albedo, emmisive - color textures viewed by human eye
};

export enum class TextureMipMode
{
	None,
	GenerateLinear,		// Standard mipmap generation (box filter)
	GenerateNormalMap,	// Normal map mipmap generation (preserves normals)
	Provided
};

export struct TextureSubresource
{
	uint32_t MipLevel = 0;
	uint32_t Layer = 0;
	uint32_t Face = 0;

	uint32_t Width = 1;
	uint32_t Height = 1;
	uint32_t Depth = 1;

	uint64_t ByteOffset = 0;
	uint64_t ByteSize = 0;
};

export struct TextureInfo
{
	uint32_t Width = 0;
	uint32_t Height = 0;
	uint32_t Depth = 1;

	uint32_t MipLevels = 1;
	uint32_t LayerCount = 1;
	uint32_t FaceCount = 1;

	vk::Format Format = vk::Format::eUndefined;
	TextureMipMode MipMode = TextureMipMode::GenerateLinear;
	SamplerDesc Sampler = PresetSamplerDesc::SamplerLinearRepeat;

	TextureColorSpace ColorSpace = TextureColorSpace::Linear;
};

export enum class TextureSourceEncoding
{
	Direct,
	BasisUniversal
};

export enum class BasisTranscodeTarget
{
	BC7RGBA,
	ASTC4x4RGBA,
	ETC2RGBA,
	BC3RGBA,
	RGBA32
};

export class TextureData : public ResourceBase
{
public:
	explicit TextureData(
		const std::string& ID,
		TextureColorSpace InColorSpace = TextureColorSpace::SRGB,
		TextureMipMode InMipMode = TextureMipMode::GenerateLinear,
		SamplerDesc InSampler = PresetSamplerDesc::SamplerLinearRepeat) :
		ResourceBase(ID),
		ImportColorSpace(InColorSpace),
		ImportMipMode(InMipMode),
		ImportSampler(InSampler)
	{
		Info = MakeImportTextureInfo();
	}

	// Accessors
	inline const std::vector<uint8_t>& GetBytes() const { return Bytes; }
	inline const std::vector<TextureSubresource>& GetSubresources() const { return Subresources; }
	inline const TextureInfo& GetInfo() const { return Info; }

	inline uint32_t GetWidth() const { return Info.Width; }
	inline uint32_t GetHeight() const { return Info.Height; }
	inline uint32_t GetDepth() const { return Info.Depth; }
	inline uint32_t GetMipLevels() const { return Info.MipLevels; }
	inline uint32_t GetLayerCount() const { return Info.LayerCount; }
	inline uint32_t GetFaceCount() const { return Info.FaceCount; }
	inline vk::Format GetFormat() const { return Info.Format; }
	inline TextureMipMode GetMipMode() const { return Info.MipMode; }
	inline const SamplerDesc& GetSamplerDesc() const { return Info.Sampler; }
	inline TextureSourceEncoding GetSourceEncoding() const { return SourceEncoding; }
	inline TextureColorSpace GetColorSpace() const { return Info.ColorSpace; }

	bool IsUploadReady() const;
	bool NeedsBasisPreparation() const;

	bool PrepareBasisPayload(BasisTranscodeTarget Target);
	bool IsPreparedFor(BasisTranscodeTarget Target) const;

	// Extension empty — ID includes extension e.g. "albedo.png"
	static std::string_view AssetFolder() { return "textures"; }
	static std::string_view FileExtension() { return ""; }

protected:
	bool LoadResource(const std::string& FilePath) override;
	bool LoadResourceFromMemory(const std::vector<uint8_t>& Data) override;
	void UnloadResource() override;

private:
	TextureInfo MakeImportTextureInfo() const;

	bool DecodeTextureData(const uint8_t* Data, std::size_t DataSize);

	bool DecodeRasterPixels(const uint8_t* Data, int DataSize);

	bool DecodeKtx2(const uint8_t* Data, std::size_t DataSize);

	TextureColorSpace ImportColorSpace;
	TextureMipMode ImportMipMode;
	SamplerDesc ImportSampler;

	std::vector<uint8_t> Bytes;
	std::vector<TextureSubresource> Subresources;
	TextureInfo Info;

	std::vector<uint8_t> EncodedSource;
	TextureSourceEncoding SourceEncoding = TextureSourceEncoding::Direct;
	std::optional<BasisTranscodeTarget> PreparedBasisTarget;
};