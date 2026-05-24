module;

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <stdexcept>

module CameraUniform;

static uint32_t FindMemoryType(const vk::raii::PhysicalDevice& PhysDev, uint32_t TypeFilter, vk::MemoryPropertyFlags Properties) {
    auto MemProps = PhysDev.getMemoryProperties();
    for (uint32_t i = 0; i < MemProps.memoryTypeCount; i++) 
    {
        if ((TypeFilter & (1 << i)) && (MemProps.memoryTypes[i].propertyFlags & Properties) == Properties)
        {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type for camera UBO");
}

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
}

void CameraUniformBuffer::Bind(vk::CommandBuffer& Cmd, vk::PipelineLayout PipelineLayout, vk::PipelineBindPoint BindPoint)
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
	uint32_t MemoryTypeIndex = FindMemoryType(PhysicalDevice, MemReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

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
	vk::DescriptorPoolCreateInfo PoolInfo({}, 1, PoolSize);
	DescriptorPool = Device.createDescriptorPool(PoolInfo);

	// Allocate descriptor set
	vk::DescriptorSetAllocateInfo AllocInfo(*DescriptorPool, *DescriptorSetLayout);
	DescriptorSet = std::move(Device.allocateDescriptorSets(AllocInfo).front());

	// Write desctriptor set to point to our buffer
	vk::DescriptorBufferInfo BufferInfo(*Buffer, 0, sizeof(CameraUniformData));
	vk::WriteDescriptorSet Write(*DescriptorSet, 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, BufferInfo);

	Device.updateDescriptorSets(Write, {});
}


