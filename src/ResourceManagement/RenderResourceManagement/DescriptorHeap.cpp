module;

#include <vulkan/vulkan_raii.hpp>

module DescriptorHeap;

import VulkanUploader;

DescriptorHeap::DescriptorHeap(
	const vk::raii::PhysicalDevice& InPhysicalDevice,
	const vk::raii::Device& InDevice,
	uint32_t InMaxTextures,
	VulkanUploader& InUploader) :
	PhysicalDevice(InPhysicalDevice),
	Device(InDevice),
	MaxTextures(InMaxTextures),
	Uploader(InUploader)
{
	FreeSlots.reserve(MaxTextures);
	for (int i = static_cast<int>(MaxTextures) - 1; i >= 0; --i)
	{
		FreeSlots.push_back(i);
	}

	// Reserve default texture slots (0, 1, 2) – these are filled externally
	// with the actual default textures.
	AllocateSlot(); // consumes index 0: TextureSlots::DefaultWhite
	AllocateSlot(); // consumes index 1: TextureSlots::DefaultNormal
	AllocateSlot(); // consumes index 2: TextureSlots::DefaultBlack

	CreateDescriptorLayout();
	CreateDescriptorPool();
	CreateDescriptorSet();
	CreateDefaultTextures();
}

void DescriptorHeap::CreateDescriptorLayout()
{
	vk::DescriptorSetLayoutBinding Binding(
		0,                                     // binding
		vk::DescriptorType::eCombinedImageSampler,
		MaxTextures,
		vk::ShaderStageFlagBits::eFragment);

	vk::DescriptorBindingFlags BindingFlags =
		vk::DescriptorBindingFlagBits::ePartiallyBound |
		vk::DescriptorBindingFlagBits::eUpdateAfterBind;

	vk::DescriptorSetLayoutBindingFlagsCreateInfo FlagsInfo(1, &BindingFlags);

	vk::DescriptorSetLayoutCreateInfo LayoutInfo(
		vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
		1, &Binding);
	LayoutInfo.setPNext(&FlagsInfo);

	Layout = Device.createDescriptorSetLayout(LayoutInfo);
}

void DescriptorHeap::CreateDescriptorPool()
{
	vk::DescriptorPoolSize PoolSize(
		vk::DescriptorType::eCombinedImageSampler,
		MaxTextures);

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

int DescriptorHeap::AllocateSlot()
{
	if (FreeSlots.empty())
	{
		return -1;
	}

	int Slot = FreeSlots.back();
	FreeSlots.pop_back();
	return Slot;
}

void DescriptorHeap::FreeSlot(int Slot)
{
	if (Slot < 0 || Slot >= static_cast<int>(MaxTextures))
	{
		throw std::out_of_range("DescriptorHeap::FreeSlot: index out of range");
	}

	FreeSlots.push_back(Slot);
}

void DescriptorHeap::WriteSlot(int Slot, vk::ImageView View, const SamplerDesc& Desc /*= PresetSamplerDesc::SamplerLinearRepeat*/)
{
	if (Slot < 0 || Slot >= static_cast<int>(MaxTextures))
	{
		throw std::out_of_range("DescriptorHeap::WriteSlot: index out of range");
	}

	vk::raii::Sampler& Sampler = GetOrCreateSampler(Desc);

	vk::DescriptorImageInfo ImageInfo(*Sampler, View, vk::ImageLayout::eShaderReadOnlyOptimal);

	vk::WriteDescriptorSet Write(
		*DescriptorSet,
		0,           // binding
		Slot,        // array element
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
	auto UploadDefault = [&](std::array<uint8_t, 4> Pixels, int Slot) -> DefaultTexture
		{
			auto Result = Uploader.UploadImage(
				Pixels.data(), 1, 1,
				vk::Format::eR8G8B8A8Unorm);

			vk::ImageViewCreateInfo ViewInfo(
				{},
				*Result.Image,
				vk::ImageViewType::e2D,
				vk::Format::eR8G8B8A8Unorm,
				{},
				{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

			vk::raii::ImageView View = Device.createImageView(ViewInfo);

			// Write into the reserved slot
			WriteSlot(Slot, *View, PresetSamplerDesc::SamplerLinearRepeat);

			DefaultTexture Tex;
			Tex.Memory = std::move(Result.Memory);
			Tex.Image = std::move(Result.Image);
			Tex.View = std::move(View);
			return Tex;
		};

	// Slot 0 — white (albedo/metallic/roughness/occlusion fallback)
	DefaultTextures[0] = UploadDefault({ 255, 255, 255, 255 }, 0);

	// Slot 1 — flat normal (0.5, 0.5, 1.0) = (128, 128, 255)
	DefaultTextures[1] = UploadDefault({ 128, 128, 255, 255 }, 1);

	// Slot 2 — black (emissive fallback)
	DefaultTextures[2] = UploadDefault({ 0, 0, 0, 255 }, 2);
}
