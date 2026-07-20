module;

#include <vulkan/vulkan_raii.hpp>
#include <memory>
#include <vector>

module Texture;

import TextureData;
import VulkanUploader;
import SamplerDesc;

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
    const SamplerDesc& Sampler = Data.GetSamplerDesc();

	// TODO: potentially support other formats in the future
    vk::Format Format = (Data.GetColorSpace() == TextureColorSpace::SRGB)
        ? vk::Format::eR8G8B8A8Srgb
        : vk::Format::eR8G8B8A8Unorm;

    VulkanUploader::ImageMipGeneration MipGeneration = Data.GetMipFilter() == TextureMipFilter::NormalMap
        ? VulkanUploader::ImageMipGeneration::NormalMapCompute
        : VulkanUploader::ImageMipGeneration::LinearBlit;

    VulkanUploader::ImageUploadInfo Info;
    Info.PixelData = Data.GetPixels().data();
    Info.Width = Data.GetWidth();
    Info.Height = Data.GetHeight();
    Info.Format = Format;
    Info.MipGeneration = MipGeneration;
    Info.AddressU = VulkanUploader::ToMipAddressMode(Sampler.AddressU);
    Info.AddressV = VulkanUploader::ToMipAddressMode(Sampler.AddressV);

    auto Result = Uploader.UploadImage(Info);

    vk::ImageViewCreateInfo ViewInfo(
        {},
        *Result.Image,
        vk::ImageViewType::e2D,
        Format,   // must match the image's actual format
        {},
        { vk::ImageAspectFlagBits::eColor, 0, Result.MipLevels, 0, 1 });

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
        const SamplerDesc& Sampler = Data->GetSamplerDesc();
        
        // TODO: potentially support other formats in the future
        vk::Format Format = (Data->GetColorSpace() == TextureColorSpace::SRGB)
            ? vk::Format::eR8G8B8A8Srgb
            : vk::Format::eR8G8B8A8Unorm;

        VulkanUploader::ImageMipGeneration MipGeneration = Data->GetMipFilter() == TextureMipFilter::NormalMap
            ? VulkanUploader::ImageMipGeneration::NormalMapCompute
            : VulkanUploader::ImageMipGeneration::LinearBlit;

        VulkanUploader::ImageUploadInfo Info;
        Info.PixelData = Data->GetPixels().data();
        Info.Width = Data->GetWidth();
        Info.Height = Data->GetHeight();
        Info.Format = Format;
        Info.MipGeneration = MipGeneration;
        Info.AddressU = VulkanUploader::ToMipAddressMode(Sampler.AddressU);
        Info.AddressV = VulkanUploader::ToMipAddressMode(Sampler.AddressV);
        UploadInfos.push_back(Info);
    }

    // Single batched GPU upload — one submit, one fence wait for all textures
    std::vector<VulkanUploader::UploadImageResult> UploadResults = Uploader.UploadImageBatch(UploadInfos);

    // Wrap each uploaded image into a Texture (creates the ImageView)
    for (size_t i = 0; i < UploadResults.size(); i++)
    {
		auto& Result = UploadResults[i];
		const auto& UploadInfo = UploadInfos[i];

        vk::ImageViewCreateInfo ViewInfo(
            {},
            *Result.Image,
            vk::ImageViewType::e2D,
            UploadInfo.Format,
            {},
            { vk::ImageAspectFlagBits::eColor, 0, Result.MipLevels, 0, 1 });

        vk::raii::ImageView View = Device.createImageView(ViewInfo);

        Results.push_back(std::unique_ptr<Texture>(new Texture(std::move(Result.Image), std::move(Result.Memory), std::move(View))));
    }

    return Results;
}
