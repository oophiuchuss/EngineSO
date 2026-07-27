module;

#include <vulkan/vulkan_raii.hpp>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

module DescriptorHeap;

import TextureSlots;
import VulkanUploader;

DescriptorHeap::DescriptorHeap(
	const vk::raii::PhysicalDevice& InPhysicalDevice,
	const vk::raii::Device& InDevice,
	uint32_t InMaxTexture2DCount,
	uint32_t InMaxCubemapCount,
	VulkanUploader& InUploader) :
	PhysicalDevice(InPhysicalDevice),
	Device(InDevice),
	MaxTexture2DCount(InMaxTexture2DCount),
	MaxCubemapCount(InMaxCubemapCount),
	Uploader(InUploader)
{
	if (MaxTexture2DCount < 3)
	{
		throw std::invalid_argument("DescriptorHeap requires at least three 2D texture slots");
	}

	if (MaxCubemapCount < 1)
	{
		throw std::invalid_argument("DescriptorHeap requires at least one cubemap slot");
	}

	FreeTexture2DSlots.reserve(MaxTexture2DCount);
	for (int Slot = static_cast<int>(MaxTexture2DCount) - 1; Slot >= 0; Slot--)
	{
		FreeTexture2DSlots.push_back(Slot);
	}

	FreeCubemapSlots.reserve(MaxCubemapCount);
	for (int Slot = static_cast<int>(MaxCubemapCount) - 1; Slot >= 0; Slot--)
	{
		FreeCubemapSlots.push_back(Slot);
	}

	// consumes index 0: TextureSlots::DefaultWhite
	const TextureDescriptorAllocation DefaultWhite = AllocateSlot(TextureDescriptorType::Texture2D);

	// consumes index 1: TextureSlots::DefaultNormal
	const TextureDescriptorAllocation DefaultNormal = AllocateSlot(TextureDescriptorType::Texture2D);

	// consumes index 2: TextureSlots::DefaultBlack
	const TextureDescriptorAllocation DefaultBlack = AllocateSlot(TextureDescriptorType::Texture2D);

	// consumes index 0: CubemapSlots::DefaultBlack
	const TextureDescriptorAllocation DefaultBlackCubemap = AllocateSlot(TextureDescriptorType::Cubemap);

	if (DefaultWhite.Slot != TextureSlots::DefaultWhite ||
		DefaultNormal.Slot != TextureSlots::DefaultNormal ||
		DefaultBlack.Slot != TextureSlots::DefaultBlack ||
		DefaultBlackCubemap.Slot != CubemapSlots::DefaultBlack)
	{
		throw std::runtime_error("DescriptorHeap default slot reservation order is invalid");
	}

	CreateDescriptorLayout();
	CreateDescriptorPool();
	CreateDescriptorSet();
	CreateDefaultTextures();
}

void DescriptorHeap::CreateDescriptorLayout()
{
	std::array<vk::DescriptorSetLayoutBinding, 2> Bindings = { {
		{
			Texture2DBinding,
			vk::DescriptorType::eCombinedImageSampler,
			MaxTexture2DCount,
			vk::ShaderStageFlagBits::eFragment
		},
		{
			CubemapBinding,
			vk::DescriptorType::eCombinedImageSampler,
			MaxCubemapCount,
			vk::ShaderStageFlagBits::eFragment
		}
	} };

	const vk::DescriptorBindingFlags CommonFlags =
		vk::DescriptorBindingFlagBits::ePartiallyBound |
		vk::DescriptorBindingFlagBits::eUpdateAfterBind;

	std::array<vk::DescriptorBindingFlags, 2> BindingFlags = { CommonFlags, CommonFlags };

	vk::DescriptorSetLayoutBindingFlagsCreateInfo FlagsInfo(BindingFlags);

	vk::DescriptorSetLayoutCreateInfo LayoutInfo(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool, Bindings);

	LayoutInfo.setPNext(&FlagsInfo);

	Layout = Device.createDescriptorSetLayout(LayoutInfo);
}

void DescriptorHeap::CreateDescriptorPool()
{
	vk::DescriptorPoolSize PoolSize(
		vk::DescriptorType::eCombinedImageSampler,
		MaxTexture2DCount + MaxCubemapCount);

	vk::DescriptorPoolCreateInfo PoolInfo(
		vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind |
		vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		1, PoolSize);

	Pool = Device.createDescriptorPool(PoolInfo);
}

void DescriptorHeap::CreateDescriptorSet()
{
	vk::DescriptorSetAllocateInfo AllocInfo(Pool, *Layout);
	DescriptorSet = std::move(Device.allocateDescriptorSets(AllocInfo).front());
}

uint32_t DescriptorHeap::GetBinding(TextureDescriptorType Type) const
{
	switch (Type)
	{
	case TextureDescriptorType::Texture2D: return Texture2DBinding;
	case TextureDescriptorType::Cubemap: return CubemapBinding;
	default:
		throw std::invalid_argument("DescriptorHeap: unsupported descriptor type");
	}
}

uint32_t DescriptorHeap::GetCapacity(TextureDescriptorType Type) const
{
	switch (Type)
	{
	case TextureDescriptorType::Texture2D: return MaxTexture2DCount;
	case TextureDescriptorType::Cubemap: return MaxCubemapCount;
	default:
		throw std::invalid_argument("DescriptorHeap: unsupported descriptor type");
	}
}

std::vector<int>& DescriptorHeap::GetFreeSlots(TextureDescriptorType Type)
{
	switch (Type)
	{
	case TextureDescriptorType::Texture2D: return FreeTexture2DSlots;
	case TextureDescriptorType::Cubemap: return FreeCubemapSlots;
	default:
		throw std::invalid_argument("DescriptorHeap: unsupported descriptor type");
	}
}

TextureDescriptorAllocation DescriptorHeap::AllocateSlot(TextureDescriptorType Type)
{
	std::vector<int>& FreeSlots = GetFreeSlots(Type);

	if (FreeSlots.empty())
	{
		return { Type, -1 };
	}

	const int Slot = FreeSlots.back();
	FreeSlots.pop_back();

	return { Type, Slot };
}

void DescriptorHeap::FreeSlot(TextureDescriptorAllocation Allocation)
{
	const uint32_t Capacity = GetCapacity(Allocation.Type);

	if (Allocation.Slot < 0 || Allocation.Slot >= static_cast<int>(Capacity))
	{
		throw std::out_of_range("DescriptorHeap::FreeSlot: index out of range");
	}

	GetFreeSlots(Allocation.Type).push_back(Allocation.Slot);
}

void DescriptorHeap::WriteSlot(TextureDescriptorAllocation Allocation, vk::ImageView View, const SamplerDesc& Desc /*= PresetSamplerDesc::SamplerLinearRepeat*/)
{
	const uint32_t Capacity = GetCapacity(Allocation.Type);

	if (Allocation.Slot < 0 || Allocation.Slot >= static_cast<int>(Capacity))
	{
		throw std::out_of_range("DescriptorHeap::WriteSlot: index out of range");
	}

	vk::raii::Sampler& Sampler = GetOrCreateSampler(Desc);

	vk::DescriptorImageInfo ImageInfo(
		*Sampler,
		View,
		vk::ImageLayout::eShaderReadOnlyOptimal);

	vk::WriteDescriptorSet Write(
		*DescriptorSet,
		GetBinding(Allocation.Type),
		Allocation.Slot,
		1,
		vk::DescriptorType::eCombinedImageSampler,
		&ImageInfo);

	Device.updateDescriptorSets(Write, {});
}

vk::Filter DescriptorHeap::ToVkFilter(FilterMode Mode)
{
	switch (Mode)
	{
	case FilterMode::Nearest: return vk::Filter::eNearest;
	case FilterMode::Linear:  return vk::Filter::eLinear;
	default:                  return vk::Filter::eLinear;
	}
}

vk::SamplerMipmapMode DescriptorHeap::ToVkMipmapMode(MipmapMode Mode)
{
	switch (Mode)
	{
	case MipmapMode::Nearest: return vk::SamplerMipmapMode::eNearest;
	case MipmapMode::Linear: return vk::SamplerMipmapMode::eLinear;
	default: return vk::SamplerMipmapMode::eLinear;
	}
}

vk::SamplerAddressMode DescriptorHeap::ToVkAddressMode(WrapMode Mode)
{
	switch (Mode)
	{
	case WrapMode::Repeat: return vk::SamplerAddressMode::eRepeat;
	case WrapMode::Clamp: return vk::SamplerAddressMode::eClampToEdge;
	case WrapMode::Mirror: return vk::SamplerAddressMode::eMirroredRepeat;
	default: return vk::SamplerAddressMode::eRepeat;
	}
}

vk::raii::Sampler& DescriptorHeap::GetOrCreateSampler(const SamplerDesc& Desc)
{
	for (auto& [Key, Sampler] : SamplerCache)
	{
		if (Key == Desc)
			return Sampler;
	}

	float ClampedMaxAniso = Desc.Anisotropy
		? (std::min)(Desc.MaxAniso, PhysicalDevice.getProperties().limits.maxSamplerAnisotropy)
		: 1.0f;

	vk::SamplerCreateInfo SamplerInfo;
	SamplerInfo
		.setMagFilter(ToVkFilter(Desc.MagFilter))
		.setMinFilter(ToVkFilter(Desc.MinFilter))
		.setMipmapMode(ToVkMipmapMode(Desc.MipMode))
		.setAddressModeU(ToVkAddressMode(Desc.AddressU))
		.setAddressModeV(ToVkAddressMode(Desc.AddressV))
		.setAddressModeW(ToVkAddressMode(Desc.AddressW))
		.setMipLodBias(Desc.MipLodBias)
		.setAnisotropyEnable(Desc.Anisotropy)
		.setMaxAnisotropy(ClampedMaxAniso)
		.setMinLod(Desc.MinLod)
		.setMaxLod(Desc.MaxLod)
		.setBorderColor(vk::BorderColor::eIntOpaqueBlack)
		.setUnnormalizedCoordinates(VK_FALSE);

	SamplerCache.emplace_back(Desc, Device.createSampler(SamplerInfo));
	return SamplerCache.back().second;
}

void DescriptorHeap::CreateDefaultTextures()
{
	auto UploadDefault2D = [&](std::array<uint8_t, 4> Pixels, int Slot) -> DefaultTexture
		{
			VulkanUploader::ImageUploadInfo Info;

			Info.Data = Pixels.data();
			Info.DataSize = Pixels.size();
			Info.Width = 1;
			Info.Height = 1;
			Info.Depth = 1;
			Info.MipLevels = 1;
			Info.ArrayLayers = 1;
			Info.Format = vk::Format::eR8G8B8A8Unorm;
			Info.MipMode = VulkanUploader::ImageMipMode::None;

			VulkanUploader::ImageSubresourceUpload Subresource;

			Subresource.BufferOffset = 0;
			Subresource.ByteSize = Pixels.size();
			Subresource.MipLevel = 0;
			Subresource.BaseArrayLayer = 0;
			Subresource.Extent = vk::Extent3D{ 1, 1, 1 };

			Info.Subresources.push_back(Subresource);

			auto Result = Uploader.UploadImage(Info);

			vk::ImageViewCreateInfo ViewInfo(
				{},
				*Result.Image,
				vk::ImageViewType::e2D,
				vk::Format::eR8G8B8A8Unorm,
				{},
				{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
			);

			vk::raii::ImageView View = Device.createImageView(ViewInfo);

			WriteSlot({ TextureDescriptorType::Texture2D, Slot }, *View, PresetSamplerDesc::SamplerLinearRepeat);

			DefaultTexture Texture;
			Texture.Memory = std::move(Result.Memory);
			Texture.Image = std::move(Result.Image);
			Texture.View = std::move(View);

			return Texture;
		};

	DefaultTexture2Ds[0] = UploadDefault2D({ 255, 255, 255, 255 }, TextureSlots::DefaultWhite);
	DefaultTexture2Ds[1] = UploadDefault2D({ 128, 128, 255, 255 }, TextureSlots::DefaultNormal);
	DefaultTexture2Ds[2] = UploadDefault2D({ 0, 0, 0, 255 }, TextureSlots::DefaultBlack);

	// Six black RGBA texels: one texel for each cubemap face.
	std::array<uint8_t, 24> CubemapPixels{};

	for (uint32_t Face = 0; Face < 6; ++Face)
	{
		CubemapPixels[Face * 4 + 3] = 255;
	}

	VulkanUploader::ImageUploadInfo CubemapInfo;

	CubemapInfo.Data = CubemapPixels.data();
	CubemapInfo.DataSize = CubemapPixels.size();
	CubemapInfo.Width = 1;
	CubemapInfo.Height = 1;
	CubemapInfo.Depth = 1;
	CubemapInfo.MipLevels = 1;
	CubemapInfo.ArrayLayers = 6;
	CubemapInfo.Format = vk::Format::eR8G8B8A8Unorm;
	CubemapInfo.MipMode = VulkanUploader::ImageMipMode::None;
	CubemapInfo.CreateFlags = vk::ImageCreateFlagBits::eCubeCompatible;

	for (uint32_t Face = 0; Face < 6; Face++)
	{
		VulkanUploader::ImageSubresourceUpload Subresource;

		Subresource.BufferOffset = static_cast<vk::DeviceSize>(Face) * 4;

		Subresource.ByteSize = 4;
		Subresource.MipLevel = 0;
		Subresource.BaseArrayLayer = Face;
		Subresource.Extent = vk::Extent3D{ 1, 1, 1 };

		CubemapInfo.Subresources.push_back(Subresource);
	}

	auto CubemapResult = Uploader.UploadImage(CubemapInfo);

	vk::ImageViewCreateInfo CubemapViewInfo(
		{},
		*CubemapResult.Image,
		vk::ImageViewType::eCube,
		vk::Format::eR8G8B8A8Unorm,
		{},
		{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6 }
	);

	vk::raii::ImageView CubemapView = Device.createImageView(CubemapViewInfo);

	WriteSlot({ TextureDescriptorType::Cubemap, CubemapSlots::DefaultBlack }, *CubemapView, PresetSamplerDesc::SamplerLinearClamp);

	DefaultCubemap.Memory = std::move(CubemapResult.Memory);
	DefaultCubemap.Image = std::move(CubemapResult.Image);
	DefaultCubemap.View = std::move(CubemapView);
}
