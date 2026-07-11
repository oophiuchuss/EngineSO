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
	std::ifstream File(FilePath, std::ios::binary || std::ios::ate);
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
	Pixels.clear();
	Info.Width = 0;
	Info.Height = 0;
	Info.Channels = 0;
	// ColorSpace and Sampler intentionally preserved since those are import-time decisions
}

bool TextureData::DecodePixels(const uint8_t* Data, int DataSize)
{
	int Width, Height, Channels;

	// Force 4 channels (RGBA) for consistent GPU upload path later
	// stb_image handles conversion from RGB, grayscale etc. automatically
	uint8_t* DecodedPixels = stbi_load_from_memory(
		Data,
		DataSize,
		&Width,
		&Height,
		&Channels,	// original channel count - stored for refference 
		4);			// requested channels - always 4 (RGBA)

	if (!DecodedPixels)
	{
		return false;
	}

	Info.Width = static_cast<uint32_t>(Width);
	Info.Height = static_cast<uint32_t>(Height);
	Info.Channels = static_cast<uint32_t>(Channels); // original, not forced 4

	size_t PixelDataSize = Width * Height * 4; // always 4 channels
	Pixels.assign(DecodedPixels, DecodedPixels + PixelDataSize);
	stbi_image_free(DecodedPixels);
	return !Pixels.empty();
}
