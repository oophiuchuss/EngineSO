module;

#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>

#include <algorithm>
#include <array>
#include <limits>

// stb_image implementation — define once here, never in a header
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

module TextureData;

namespace
{
	constexpr std::array<uint8_t, 12> Ktx2Identifier = {
		0xAB, 0x4B, 0x54, 0x58,
		0x20, 0x32, 0x30, 0xBB,
		0x0D, 0x0A, 0x1A, 0x0A
	};

	bool HasKtx2Identifier(const uint8_t* Data, size_t DataSize)
	{
		if (!Data || DataSize < Ktx2Identifier.size())
		{
			return false;
		}

		return std::equal(Ktx2Identifier.begin(), Ktx2Identifier.end(), Data);
	}
}

bool TextureData::IsUploadReady() const
{
	return !Bytes.empty() && !Subresources.empty() && Info.Format != vk::Format::eUndefined;
}

bool TextureData::NeedsBasisPreparation() const
{
	return SourceEncoding == TextureSourceEncoding::BasisUniversal && !IsUploadReady();
}

bool TextureData::IsPreparedFor(BasisTranscodeTarget Target) const
{
	return PreparedBasisTarget.has_value() && PreparedBasisTarget.value() == Target && IsUploadReady();
}

bool TextureData::LoadResource(const std::string& FilePath)
{
	std::ifstream File(FilePath, std::ios::binary | std::ios::ate);

	if (!File)
	{
		return false;
	}

	const std::streamsize FileSize = File.tellg();

	if (FileSize <= 0)
	{
		return false;
	}

	std::vector<uint8_t> RawData(static_cast<size_t>(FileSize));

	File.seekg(0, std::ios::beg);

	if (!File.read(reinterpret_cast<char*>(RawData.data()),FileSize))
	{
		return false;
	}

	return DecodeTextureData(RawData.data(), RawData.size());
}

bool TextureData::LoadResourceFromMemory(const std::vector<uint8_t>& Data)
{
	return DecodeTextureData(Data.data(),Data.size());
}

void TextureData::UnloadResource()
{
	Bytes.clear();
	Subresources.clear();
	EncodedSource.clear();

	SourceEncoding = TextureSourceEncoding::Direct;
	PreparedBasisTarget.reset();

	Info = MakeImportTextureInfo();
	Info.MipLevels = 0;
}

TextureInfo TextureData::MakeImportTextureInfo() const
{
	TextureInfo Result;

	Result.Format = ImportColorSpace == TextureColorSpace::SRGB ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

	Result.MipMode = ImportMipMode;
	Result.Sampler = ImportSampler;
	Result.ColorSpace = ImportColorSpace;

	return Result;
}

bool TextureData::DecodeTextureData(const uint8_t* Data, std::size_t DataSize)
{
	if (!Data || DataSize == 0)
	{
		return false;
	}

	if (HasKtx2Identifier(Data, DataSize))
	{
		return DecodeKtx2(Data, DataSize);
	}

	if (DataSize > static_cast<size_t>((std::numeric_limits<int>::max)()))
	{
		return false;
	}

	return DecodeRasterPixels(Data, static_cast<int>(DataSize));
}

bool TextureData::DecodeRasterPixels(const uint8_t* Data, int DataSize)
{
    int Width = 0;
    int Height = 0;

    uint8_t* DecodedPixels = stbi_load_from_memory(Data, DataSize, &Width, &Height, nullptr, 4);

    if (!DecodedPixels || Width <= 0 || Height <= 0)
    {
        if (DecodedPixels)
        {
            stbi_image_free(DecodedPixels);
        }

        return false;
    }

	Info = MakeImportTextureInfo();

	Info.Width = static_cast<uint32_t>(Width);
	Info.Height = static_cast<uint32_t>(Height);
	Info.Depth = 1;

	Info.MipLevels = 1;
	Info.LayerCount = 1;
	Info.FaceCount = 1;

    const size_t ByteCount = static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4;

    Bytes.assign(DecodedPixels, DecodedPixels + ByteCount);

    stbi_image_free(DecodedPixels);

    TextureSubresource BaseLevel;
    BaseLevel.MipLevel = 0;
    BaseLevel.Layer = 0;
    BaseLevel.Face = 0;
    BaseLevel.Width = Info.Width;
    BaseLevel.Height = Info.Height;
    BaseLevel.Depth = 1;
    BaseLevel.ByteOffset = 0;
    BaseLevel.ByteSize = static_cast<uint64_t>(Bytes.size());

    Subresources.clear();
    Subresources.push_back(BaseLevel);

	EncodedSource.clear();
	SourceEncoding = TextureSourceEncoding::Direct;
	PreparedBasisTarget.reset();

    return !Bytes.empty();
}
