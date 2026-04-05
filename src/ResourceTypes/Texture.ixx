module;

#include <optional>
#include <string>
#include <vulkan/vulkan_raii.hpp>

export module Texture;

import ResourceBase;

export class Texture : public ResourceBase
{
public:
	explicit Texture(const std::string& id) : ResourceBase(id) {}
	
	~Texture() 
	{ 
		Unload();
	}

	// Accessors for Vulkan image, image view, and sampler handles, returning default-constructed handles if not loaded
	vk::Image GetVulkanImage() const { return Image ? **Image : vk::Image{}; }
	vk::ImageView GetVulkanImageView() const { return ImageView ? **ImageView : vk::ImageView{}; }
	vk::Sampler GetVulkanSampler() const { return Sampler ? **Sampler : vk::Sampler{}; }

protected:
	bool LoadResource(const std::string& FilePath) override;
	void UnloadResource() override;

private:
	// TODO: Implement actual image loading, Vulkan image creation, and memory management logic
	// Placeholder functions for image loading and Vulkan resource creation
	unsigned char* LoadImageData(const std::string& FilePath, int* OutWidth, int* OutHeight, int* OutChannels) { return nullptr; }
	void CreateVulkanImage(unsigned char* Data, int Width, int Height, int Channels) {}
	void FreeImageData(unsigned char* Data) {}
	vk::Device GetDevice() const { return vk::Device(); }

	// Vulkan handles for the texture resource
	std::optional<vk::raii::Image> Image;				// Vulkan image handle containing pixel data
	std::optional<vk::raii::DeviceMemory> ImageMemory;	// Memory allocated for the image
	std::optional<vk::raii::ImageView> ImageView;		// Shader -accessible view of the image (format, subresource range, etc.)
	std::optional<vk::raii::Sampler> Sampler;			// Sampling configuration for the texture (filtering, wrapping, etc.)
	vk::DeviceSize ImageOffset;							// Offset within the allocated memory where the image data starts

	// Texture metadata for validation and debugging
	int Width = 0;		// Width of the texture in pixels
	int Height = 0;		// Height of the texture in pixels
	int Channels = 0;	// Number of color channels (e.g., 4 for RGBA)
};