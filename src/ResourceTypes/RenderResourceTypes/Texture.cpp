module;

#include <vulkan/vulkan_raii.hpp>

module Texture;

import TextureData;
import VulkanUploader;

Texture::Texture(vk::raii::Image&& InImage,
    vk::raii::DeviceMemory&& InMemory,
    vk::raii::ImageView&& InView) :
    Image(std::move(InImage)) ,
    ImageMemory(std::move(InMemory)) ,
    ImageView(std::move(InView))
{
}

std::unique_ptr<Texture> Texture::CreateFromTextureData(const vk::raii::Device& Device, const vk::raii::PhysicalDevice& PhysicalDevice, VulkanUploader& Uploader, const TextureData& Data)
{
    auto Result = Uploader.UploadImage(
        Data.GetPixels().data(),
        Data.GetWidth(),
        Data.GetHeight(),
        vk::Format::eR8G8B8A8Unorm);

    vk::ImageViewCreateInfo ViewInfo(
        {},
        *Result.Image,
        vk::ImageViewType::e2D,
        vk::Format::eR8G8B8A8Unorm,
        {},
        { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

    vk::raii::ImageView View = Device.createImageView(ViewInfo);

    return std::unique_ptr<Texture>(new Texture(std::move(Result.Image), std::move(Result.Memory), std::move(View)));
}
