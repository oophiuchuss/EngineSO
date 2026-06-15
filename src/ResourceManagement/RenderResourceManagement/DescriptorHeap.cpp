module;

#include <vulkan/vulkan_raii.hpp>

module DescriptorHeap;

DescriptorHeap::DescriptorHeap(
	const vk::raii::Device& InDevice,
	uint32_t InMaxTextures) :
	Device(InDevice),
	MaxTextures(InMaxTextures)
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

vk::raii::Sampler& DescriptorHeap::GetOrCreateSampler(const SamplerDesc& Desc)
{
	for (auto& [Key, Sampler] : SamplerCache)
	{
		if (Key == Desc)
		{
			return Sampler;
		}
	}

	vk::SamplerCreateInfo SamplerInfo;
	SamplerInfo.setMagFilter(Desc.MagFilter)
		.setMinFilter(Desc.MinFilter)
		.setMipmapMode(Desc.MipmapMode)
		.setAddressModeU(Desc.AddressModeU)
		.setAddressModeV(Desc.AddressModeV)
		.setAddressModeW(Desc.AddressModeW)
		.setMipLodBias(Desc.MipLodBias)
		.setAnisotropyEnable(Desc.AnisotropyEnable)
		.setMaxAnisotropy(Desc.MaxAnisotropy)
		.setMinLod(Desc.MinLod)
		.setMaxLod(Desc.MaxLod)
		.setBorderColor(vk::BorderColor::eIntOpaqueBlack)
		.setUnnormalizedCoordinates(VK_FALSE);

	SamplerCache.emplace_back(Desc, Device.createSampler(SamplerInfo));
	return SamplerCache.back().second;
}