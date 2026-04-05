module;

#include <optional>
#include <string>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan.hpp>

module Texture;

bool Texture::LoadResource(const std::string& FilePath)
{
	// Load raw image data from the file
	int Width_Tmp = 0;
	int Height_Tmp = 0;
	int Channels_Tmp = 0;
	unsigned char* Data = LoadImageData(FilePath, &Width_Tmp, &Height_Tmp, &Channels_Tmp);
	if (!Data)
	{
		return false; // Failed to load image data
	}

	// Store the loaded texture metadata for later use
	Width = Width_Tmp;
	Height = Height_Tmp;
	Channels = Channels_Tmp;

	// Create a Vulkan image resource using the loaded data and texture metadata
	CreateVulkanImage(Data, Width, Height, Channels);

	// Free the raw image data after creating the Vulkan image, as it's now stored in GPU memory
	FreeImageData(Data);

	return true; // Result will mark the resource as loaded
}

void Texture::UnloadResource()
{
	Sampler.reset();
	ImageView.reset();
	Image.reset();
	ImageMemory.reset();
	Width = 0;
	Height = 0;
	Channels = 0;
	ImageOffset = 0;
}
