module;

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <memory>

module Mesh;

import Geometry;
import VulkanUtils;

VertexBuffer::VertexBuffer(vk::raii::Buffer&& Buffer,
						   vk::raii::DeviceMemory&& DeviceMemory,
						   uint32_t VertexCount,
						   uint8_t Stride)
	: Buffer(std::move(Buffer)),
	BufferMemory(std::move(DeviceMemory)),
	VertexCount(VertexCount),
	Stride(Stride)
{
}

std::unique_ptr<VertexBuffer> VertexBuffer::Create(
	const vk::raii::Device& Device, 
	const vk::raii::PhysicalDevice& PhysicalDevice, 
	const std::vector<uint8_t>& Data, 
	uint8_t Stride)
{
	vk::BufferCreateInfo BufferInfo({}, Data.size(),
		vk::BufferUsageFlagBits::eVertexBuffer,
		vk::SharingMode::eExclusive);

	vk::raii::Buffer Buffer = Device.createBuffer(BufferInfo);

	auto MemReqs = Buffer.getMemoryRequirements();

	// Find a suitable memory type index for the buffer
	uint32_t MemoryTypeIndex = VulkanUtils::FindMemoryType(
															PhysicalDevice,
															MemReqs.memoryTypeBits,
															vk::MemoryPropertyFlagBits::eHostVisible | 
															vk::MemoryPropertyFlagBits::eHostCoherent);

	vk::MemoryAllocateInfo AllocInfo(MemReqs.size, MemoryTypeIndex);
	vk::raii::DeviceMemory Memory = Device.allocateMemory(AllocInfo);

	// Bind buffer to memory and map it for updates
	Buffer.bindMemory(*Memory, 0);

	void* MappedMemory = Memory.mapMemory(0, Data.size(), {});
	std::memcpy(MappedMemory, Data.data(), Data.size());
	Memory.unmapMemory();

	return std::unique_ptr<VertexBuffer>(
		new VertexBuffer(std::move(Buffer), std::move(Memory),
			static_cast<uint32_t>(Data.size() / Stride), Stride));
}

void VertexBuffer::Bind(const vk::raii::CommandBuffer& CommandBuffer, uint32_t binding) const
{
	vk::DeviceSize Offset = 0; // TODO: fix for now assuming single vertex buffer with no offsets for simplicity
	CommandBuffer.bindVertexBuffers(binding, { *Buffer }, { Offset });
}

IndexBuffer::IndexBuffer(vk::raii::Buffer&& Buffer,
						 vk::raii::DeviceMemory&& DeviceMemory,
						 uint32_t IndexCount)
	: Buffer(std::move(Buffer)),
	BufferMemory(std::move(DeviceMemory)),
	IndexCount(IndexCount)
{
}

std::unique_ptr<IndexBuffer> IndexBuffer::Create(const vk::raii::Device& Device, const vk::raii::PhysicalDevice& PhysicalDevice, const std::vector<uint32_t>& Indices)
{
	vk::DeviceSize DataSize = sizeof(uint32_t) * Indices.size();

	vk::BufferCreateInfo BufferInfo({}, DataSize,
		vk::BufferUsageFlagBits::eIndexBuffer,
		vk::SharingMode::eExclusive);

	vk::raii::Buffer Buffer = Device.createBuffer(BufferInfo);

	auto MemReqs = Buffer.getMemoryRequirements();

	// Find a suitable memory type index for the buffer
	uint32_t MemoryTypeIndex = VulkanUtils::FindMemoryType(
		PhysicalDevice,
		MemReqs.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent);

	vk::MemoryAllocateInfo AllocInfo(MemReqs.size, MemoryTypeIndex);
	vk::raii::DeviceMemory Memory = Device.allocateMemory(AllocInfo);

	// Bind buffer to memory and map it for updates
	Buffer.bindMemory(*Memory, 0);

	void* MappedMemory = Memory.mapMemory(0, DataSize, {});
	std::memcpy(MappedMemory, Indices.data(), DataSize);
	Memory.unmapMemory();

	return std::unique_ptr<IndexBuffer>(
		new IndexBuffer(std::move(Buffer), std::move(Memory),
			static_cast<uint32_t>(Indices.size())));
}

void IndexBuffer::Bind(const vk::raii::CommandBuffer& CommandBuffer, uint32_t binding) const
{
	CommandBuffer.bindIndexBuffer(*Buffer, 0, vk::IndexType::eUint32); // TODO: fix for now assuming single index buffer with no offsets for simplicity
}

Mesh::Mesh(	std::unique_ptr<VertexBuffer> VertexBuffer,
			std::unique_ptr<IndexBuffer> IndexBuffer,
			const BoundingBox& Bounds)
	: VertexBufferPtr(std::move(VertexBuffer)),
	IndexBufferPtr(std::move(IndexBuffer)),
	Bounds(Bounds)
{
}

std::unique_ptr<Mesh> Mesh::CreateTestTriangle(const vk::raii::Device& Device, const vk::raii::PhysicalDevice& PhysicalDevice)
{
	// World-space triangle vertices 
	std::vector<Vertex> Verticies = {
		{ glm::vec3(0.0f, -0.5f, -5.0f) },
		{ glm::vec3(0.5f,  0.5f, -5.0f) },
		{ glm::vec3(-0.5f,  0.5f, -5.0f) }
	};

	// Index drawing
	std::vector<uint32_t> Indices = { 0, 2, 1 };

	// Convert vertex data to byte array for buffer creation
	std::vector<uint8_t> VertexData(Verticies.size() * sizeof(Vertex));
	std::memcpy(VertexData.data(), Verticies.data(), VertexData.size());

	// Create vertex and index buffers
	auto VertexBufferPtr = VertexBuffer::Create(Device, PhysicalDevice, VertexData, sizeof(Vertex));
	auto IndexBufferPtr = IndexBuffer::Create(Device, PhysicalDevice, Indices);

	// Calculate bounding box
	BoundingBox Bounds;
	Bounds.Min = glm::vec3( -0.5f, -0.5f, 0.0f );
	Bounds.Max = glm::vec3( 0.5f, 0.5f, 0.0f );

	return std::unique_ptr<Mesh>(
		new Mesh(std::move(VertexBufferPtr), std::move(IndexBufferPtr), Bounds));
}



/*bool Mesh::LoadResource(const std::string& FilePath)
{
	// Load the mesh data from the file, including vertex attributes and indices
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;
	if (!LoadMeshData(FilePath, Vertices, Indices))
	{
		return false; // Failed to load mesh data
	}
	
	// Create Vulkan buffers for the vertex and index data, and upload the data to GPU memory
	CreateVertexBuffer(Vertices);
	CreateIndexBuffer(Indices);

	// Cache metadata about the vertex and index counts for later use during rendering
	VertexCount = static_cast<uint32_t>(Vertices.size());
	IndexCount = static_cast<uint32_t>(Indices.size());

	return true; // Result will mark the resource as loaded
}

void Mesh::UnloadResource()
{
	VertexBuffer.reset();
	VertexBufferMemory.reset();
	VertexBufferOffset = 0;
	VertexCount = 0;

	IndexBuffer.reset();
	IndexBufferMemory.reset();
	IndexBufferOffset = 0;
	IndexCount = 0;
}*/
