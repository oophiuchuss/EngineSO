module;

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

module CameraUniform;
import VulkanUtils;

CameraUniformBuffer::CameraUniformBuffer(
    const vk::raii::Device& Device, 
    const vk::raii::PhysicalDevice& PhysicalDevice,
    uint32_t FramesInFlight) : 
    Device(Device), 
    PhysicalDevice(PhysicalDevice),
    FramesInFlight(FramesInFlight)
{
	if (FramesInFlight == 0)
	{
		throw std::invalid_argument("CameraUniformBuffer: FramesInFlight must be greater than zero");
	}

	CreateBuffer();
	CreateDescriptor();
}

CameraUniformBuffer::~CameraUniformBuffer()
{
	for (FrameResources& Frame : Frames)
	{
		if (Frame.MappedMemory)
		{
			Frame.Memory.unmapMemory();
			Frame.MappedMemory = nullptr;
		}
	}
}

void CameraUniformBuffer::Update(uint32_t FrameIndex, const CameraUniformData& Data)
{
	// Copy data to mapped memory
	FrameResources& Frame = GetFrameResources(FrameIndex);

	std::memcpy(Frame.MappedMemory, &Data, sizeof(CameraUniformData));

	LastData = Data;
}

const vk::raii::DescriptorSet& CameraUniformBuffer::GetDescriptorSet(uint32_t FrameIndex) const
{
	return GetFrameResources(FrameIndex).DescriptorSet;
}

void CameraUniformBuffer::CreateBuffer()
{
	Frames.resize(FramesInFlight);

	for (FrameResources& Frame : Frames)
	{
		// Map memory and copy data
		vk::BufferCreateInfo BufferInfo({}, sizeof(CameraUniformData),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::SharingMode::eExclusive);

		Frame.Buffer = Device.createBuffer(BufferInfo);

		// Get memory requirements and allocate memory
		auto MemReqs = Frame.Buffer.getMemoryRequirements();
		uint32_t MemoryTypeIndex = VulkanUtils::FindMemoryType(PhysicalDevice, MemReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		vk::MemoryAllocateInfo AllocInfo(MemReqs.size, MemoryTypeIndex);

		Frame.Memory = Device.allocateMemory(AllocInfo);

		// Bind buffer to memory and map it for updates
		Frame.Buffer.bindMemory(*Frame.Memory, 0);
		Frame.MappedMemory = Frame.Memory.mapMemory(0, sizeof(CameraUniformData));
	}
}

void CameraUniformBuffer::CreateDescriptor()
{
	// Descriptor set layout
	vk::DescriptorSetLayoutBinding LayoutBinding(
		0,
		vk::DescriptorType::eUniformBuffer,
        1,
		vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
	);

    vk::DescriptorSetLayoutCreateInfo LayoutInfo({}, LayoutBinding);
	DescriptorSetLayout = Device.createDescriptorSetLayout(LayoutInfo);

	vk::DescriptorPoolSize PoolSize(vk::DescriptorType::eUniformBuffer, FramesInFlight);
	vk::DescriptorPoolCreateInfo PoolInfo(
		vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		FramesInFlight, PoolSize);

	DescriptorPool = Device.createDescriptorPool(PoolInfo);

	std::vector<vk::DescriptorSetLayout> Layouts(FramesInFlight, *DescriptorSetLayout);

	vk::DescriptorSetAllocateInfo AllocateInfo(*DescriptorPool, Layouts);

	std::vector<vk::raii::DescriptorSet> DescriptorSets = Device.allocateDescriptorSets(AllocateInfo);

	for (uint32_t FrameIndex = 0; FrameIndex < FramesInFlight; FrameIndex++)
	{
		FrameResources& Frame = Frames[FrameIndex];

		Frame.DescriptorSet = std::move(DescriptorSets[FrameIndex]);

		// Write desctriptor set to point to our buffer
		vk::DescriptorBufferInfo BufferInfo(*Frame.Buffer, 0, sizeof(CameraUniformData));
		vk::WriteDescriptorSet Write(*Frame.DescriptorSet, 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, BufferInfo);

		Device.updateDescriptorSets(Write, {});
	}
}


CameraUniformBuffer::FrameResources& CameraUniformBuffer::GetFrameResources(uint32_t FrameIndex)
{
	if (FrameIndex >= Frames.size())
	{
		throw std::out_of_range("CameraUniformBuffer: frame index is out of range");
	}

	return Frames[FrameIndex];
}

const CameraUniformBuffer::FrameResources& CameraUniformBuffer::GetFrameResources(uint32_t FrameIndex) const
{
	if (FrameIndex >= Frames.size())
	{
		throw std::out_of_range("CameraUniformBuffer: frame index is out of range");
	}

	return Frames[FrameIndex];
}


