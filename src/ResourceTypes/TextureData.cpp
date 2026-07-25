module;

#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>

// stb_image implementation — define once here, never in a header
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

module TextureData;


bool TextureData::LoadResource(const std::string& FilePath)
{
	std::ifstream File(FilePath, std::ios::binary | std::ios::ate);
	if (!File)
	{
		return false;
	}

	auto FileSize = File.tellg();
	std::vector<uint8_t> RawData(static_cast<size_t>(FileSize));
	File.seekg(0);
	File.read(reinterpret_cast<char*>(RawData.data()), FileSize);

	return DecodePixels(RawData.data(), static_cast<int>(RawData.size()));
}

bool TextureData::LoadResourceFromMemory(const std::vector<uint8_t>& Data)
{
	return DecodePixels(Data.data(), static_cast<int>(Data.size()));
}

void TextureData::UnloadResource()
{
	Bytes.clear();
	Subresources.clear();

	Info.Width = 0;
	Info.Height = 0;
	Info.Depth = 1;

	Info.MipLevels = 0;
	Info.LayerCount = 1;
	Info.FaceCount = 1;

	// Format, mip policy and sampler are import decisions and remain unchanged across reloads.
}

bool TextureData::DecodePixels(const uint8_t* Data, int DataSize)
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

    return !Bytes.empty();
}
