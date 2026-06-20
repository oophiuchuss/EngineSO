module;

#include <vulkan/vulkan_raii.hpp>
#include <memory>
#include <vector>

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

std::unique_ptr<Texture> Texture::CreateFromTextureData(const vk::raii::Device& Device, VulkanUploader& Uploader, const TextureData& Data)
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

std::vector<std::unique_ptr<Texture>> Texture::CreateBatchFromTextureData(const vk::raii::Device& Device, VulkanUploader& Uploader, const std::vector<const TextureData*>& DataList)
{
    std::vector<std::unique_ptr<Texture>> Results;
    Results.reserve(DataList.size());

    if (DataList.empty())
        return Results;

    // Build the batch upload request — one entry per texture
    std::vector<VulkanUploader::ImageUploadInfo> UploadInfos;
    UploadInfos.reserve(DataList.size());

    for (const TextureData* Data : DataList)
    {
        VulkanUploader::ImageUploadInfo Info;
        Info.PixelData = Data->GetPixels().data();
        Info.Width = Data->GetWidth();
        Info.Height = Data->GetHeight();
        Info.Format = vk::Format::eR8G8B8A8Unorm; // TODO: respect color space later
        UploadInfos.push_back(Info);
    }

    // Single batched GPU upload — one submit, one fence wait for all textures
    std::vector<VulkanUploader::UploadImageResult> UploadResults =
        Uploader.UploadImageBatch(UploadInfos);

    // Wrap each uploaded image into a Texture (creates the ImageView)
    for (auto& Result : UploadResults)
    {
        vk::ImageViewCreateInfo ViewInfo(
            {},
            *Result.Image,
            vk::ImageViewType::e2D,
            vk::Format::eR8G8B8A8Unorm,
            {},
            { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

        vk::raii::ImageView View = Device.createImageView(ViewInfo);

        Results.push_back(std::unique_ptr<Texture>(new Texture(std::move(Result.Image), std::move(Result.Memory), std::move(View))));
    }

    return Results;
}
