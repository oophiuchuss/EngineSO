module;

#include <vulkan/vulkan_raii.hpp>
#include <memory>
#include <vector>

export module Texture;

import TextureData;
import VulkanUploader;

export class Texture
{
public:
	Texture() = default;
	~Texture() = default;

	// Non-copyable and movable semantics to manage Vulkan resources safely
	Texture(const Texture&) = delete;					// Prevent copying to avoid double-free issues with Vulkan resources
	Texture& operator=(const Texture&) = delete;		// Prevent copy assignment for the same reason
	Texture(Texture&&) noexcept = default;				// Allow move construction to transfer ownership of Vulkan resources without copying
	Texture& operator=(Texture&&) noexcept = default;	// Allow move assignment for the same reason

	static std::unique_ptr<Texture> CreateFromTextureData(
		const vk::raii::Device& Device,
		VulkanUploader& Uploader,
		const TextureData& Data);

	// Batch variant that uploads many textures in a single command buffer submission
	static std::vector<std::unique_ptr<Texture>> CreateBatchFromTextureData(
		const vk::raii::Device& Device,
		VulkanUploader& Uploader,
		const std::vector<const TextureData*>& DataList);


	vk::Image GetImage() const { return *Image; }
	vk::ImageView GetImageView() const { return *ImageView; }

private:
	// Private constructor to enforce creation through the factory method, ensuring proper Vulkan resource management
	Texture(vk::raii::Image&& InImage,
		vk::raii::DeviceMemory&& InImageMemory,
		vk::raii::ImageView&& InImageView);

	vk::raii::Image Image = nullptr;
	vk::raii::DeviceMemory ImageMemory = nullptr;
	vk::raii::ImageView ImageView = nullptr;
};

