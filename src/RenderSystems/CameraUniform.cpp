module;

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <stdexcept>

module CameraUniform;
import VulkanUtils;

CameraUniformBuffer::CameraUniformBuffer(
    const vk::raii::Device& Device, 
    const vk::raii::PhysicalDevice& PhysicalDevice) : 
    Device(Device), 
    PhysicalDevice(PhysicalDevice)
{
	CreateBuffer();
	CreateDescriptor();
}

void CameraUniformBuffer::Update(const CameraUniformData& Data)
{
	// Copy data to mapped memory
	std::memcpy(MappedMemory, &Data, sizeof(CameraUniformData));
	LastData = Data;
}

void CameraUniformBuffer::Bind(vk::CommandBuffer Cmd, vk::PipelineLayout PipelineLayout, vk::PipelineBindPoint BindPoint)
{
	Cmd.bindDescriptorSets(BindPoint, PipelineLayout, 0, { *DescriptorSet }, nullptr);
}

void CameraUniformBuffer::CreateBuffer()
{
    // Map memory and copy data
    vk::BufferCreateInfo BufferInfo({}, sizeof(CameraUniformData),
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::SharingMode::eExclusive);

	Buffer = Device.createBuffer(BufferInfo);

	// Get memory requirements and allocate memory
    auto MemReqs = Buffer.getMemoryRequirements();
	uint32_t MemoryTypeIndex = VulkanUtils::FindMemoryType(PhysicalDevice, MemReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	vk::MemoryAllocateInfo AllocInfo(MemReqs.size, MemoryTypeIndex);

	Memory = Device.allocateMemory(AllocInfo);
	
	// Bind buffer to memory and map it for updates
    Buffer.bindMemory(*Memory, 0);
	MappedMemory = Memory.mapMemory(0, sizeof(CameraUniformData));
}

void CameraUniformBuffer::CreateDescriptor()
{
	// Descriptor set layout
	vk::DescriptorSetLayoutBinding LayoutBinding(0, vk::DescriptorType::eUniformBuffer,
                                                 1, vk::ShaderStageFlagBits::eVertex);

    vk::DescriptorSetLayoutCreateInfo LayoutInfo({}, LayoutBinding);
	DescriptorSetLayout = Device.createDescriptorSetLayout(LayoutInfo);

	// Descriptor pool (support one UBO)
	vk::DescriptorPoolSize PoolSize(vk::DescriptorType::eUniformBuffer, 1);
	vk::DescriptorPoolCreateInfo PoolInfo(
		vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		1, PoolSize);
	DescriptorPool = Device.createDescriptorPool(PoolInfo);

	// Allocate descriptor set
	vk::DescriptorSetAllocateInfo AllocInfo(*DescriptorPool, *DescriptorSetLayout);
	DescriptorSet = std::move(Device.allocateDescriptorSets(AllocInfo).front());

	// Write desctriptor set to point to our buffer
	vk::DescriptorBufferInfo BufferInfo(*Buffer, 0, sizeof(CameraUniformData));
	vk::WriteDescriptorSet Write(*DescriptorSet, 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, BufferInfo);

	Device.updateDescriptorSets(Write, {});
}


