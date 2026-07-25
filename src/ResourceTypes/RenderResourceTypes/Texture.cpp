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
    VulkanUploader::ImageUploadInfo Info = GenerateVulkanUploaderImageInfo(Data);

    auto Result = Uploader.UploadImage(Info);

    vk::ImageViewType ViewType;

    if (Data.GetFaceCount() == 6)
    {
        ViewType = Data.GetLayerCount() == 1 ? vk::ImageViewType::eCube : vk::ImageViewType::eCubeArray;
    }
    else
    {
        ViewType = Data.GetLayerCount() == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray;
    }

    vk::ImageViewCreateInfo ViewInfo(
        {},
        *Result.Image,
        ViewType,
        Info.Format,
        {},
        {
            vk::ImageAspectFlagBits::eColor,
            0,
            Result.MipLevels,
            0,
            Result.ArrayLayers
        });

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
        UploadInfos.push_back(GenerateVulkanUploaderImageInfo(*Data));
    }

    // Single batched GPU upload — one submit, one fence wait for all textures
    std::vector<VulkanUploader::UploadImageResult> UploadResults = Uploader.UploadImageBatch(UploadInfos);

    // Wrap each uploaded image into a Texture (creates the ImageView)
    for (size_t i = 0; i < UploadResults.size(); i++)
    {
		auto& Result = UploadResults[i];
		const auto& UploadInfo = UploadInfos[i];
		const auto* Data = DataList[i];

        vk::ImageViewType ViewType;

        if (Data->GetFaceCount() == 6)
        {
            ViewType = Data->GetLayerCount() == 1 ? vk::ImageViewType::eCube : vk::ImageViewType::eCubeArray;
        }
        else
        {
            ViewType = Data->GetLayerCount() == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray;
        }

        vk::ImageViewCreateInfo ViewInfo(
            {},
            *Result.Image,
            ViewType,
            UploadInfo.Format,
            {},
        {
            vk::ImageAspectFlagBits::eColor,
            0,
            Result.MipLevels,
            0,
            Result.ArrayLayers
        });

        vk::raii::ImageView View = Device.createImageView(ViewInfo);

        Results.push_back(std::unique_ptr<Texture>(new Texture(std::move(Result.Image), std::move(Result.Memory), std::move(View))));
    }

    return Results;
}

VulkanUploader::ImageUploadInfo Texture::GenerateVulkanUploaderImageInfo(const TextureData& Data)
{
    if (Data.GetFaceCount() != 1 && Data.GetFaceCount() != 6)
    {
        throw std::runtime_error("FaceCount must be either 1 or 6");
    }

    if (Data.GetFaceCount() == 6 && Data.GetWidth() != Data.GetHeight())
    {
        throw std::runtime_error("Cubemap faces must be square");
    }

    const SamplerDesc& Sampler = Data.GetSamplerDesc();

	VulkanUploader::ImageUploadInfo Info;

    Info.Data = Data.GetBytes().data();
    Info.DataSize = Data.GetBytes().size();

    Info.Width = Data.GetWidth();
    Info.Height = Data.GetHeight();
    Info.Depth = Data.GetDepth();

    Info.MipLevels = Data.GetMipLevels();
    Info.ArrayLayers = Data.GetLayerCount() * Data.GetFaceCount();

    Info.Format = Data.GetFormat();
    Info.MipMode = ToVulkanMipMode(Data.GetMipMode());

    if (Data.GetFaceCount() == 6)
    {
        Info.CreateFlags |= vk::ImageCreateFlagBits::eCubeCompatible;
    }

    Info.AddressU = VulkanUploader::ToMipAddressMode(Sampler.AddressU);
    Info.AddressV = VulkanUploader::ToMipAddressMode(Sampler.AddressV);

    for (const TextureSubresource& Source : Data.GetSubresources())
    {
        VulkanUploader::ImageSubresourceUpload Upload;

        Upload.BufferOffset = Source.ByteOffset;
        Upload.ByteSize = Source.ByteSize;
        Upload.MipLevel = Source.MipLevel;

        Upload.BaseArrayLayer = Source.Layer * Data.GetFaceCount() + Source.Face;

        Upload.Extent = vk::Extent3D{ Source.Width, Source.Height, Source.Depth };

        Info.Subresources.push_back(Upload);
    }

	return Info;
}

VulkanUploader::ImageMipMode Texture::ToVulkanMipMode(TextureMipMode MipMode)
{
    switch (MipMode)
    {
    case TextureMipMode::GenerateLinear:
        return VulkanUploader::ImageMipMode::GenerateLinear;
    case TextureMipMode::GenerateNormalMap:
        return VulkanUploader::ImageMipMode::GenerateNormalMap;
	case TextureMipMode::None:
		return VulkanUploader::ImageMipMode::None;
	case TextureMipMode::Provided:
		return VulkanUploader::ImageMipMode::Provided;
    default:
        throw std::runtime_error("Unsupported TextureMipMode for Vulkan upload");
    }
}
