module;

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <memory>

module Mesh;

import Geometry;
import VulkanUtils;
import VulkanUploader;

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
	VulkanUploader* Uploader,
	const std::vector<uint8_t>& Data, 
	uint8_t Stride)
{
	// Upload directly to device-local memory via staging buffer
	auto Result = Uploader->UploadBuffer(
		Data.data(),
		Data.size(),
		vk::BufferUsageFlagBits::eVertexBuffer);

	return std::unique_ptr<VertexBuffer>(
		new VertexBuffer(
			std::move(Result.Buffer),
			std::move(Result.Memory),
			static_cast<uint32_t>(Data.size() / Stride), 
			Stride));
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

std::unique_ptr<IndexBuffer> IndexBuffer::Create(
	const vk::raii::Device& Device, 
	const vk::raii::PhysicalDevice& PhysicalDevice, 
	VulkanUploader* Uploader, 
	const std::vector<uint32_t>& Indices)
{
	vk::DeviceSize DataSize = sizeof(uint32_t) * Indices.size();

	auto Result = Uploader->UploadBuffer(
		Indices.data(),
		DataSize,
		vk::BufferUsageFlagBits::eIndexBuffer);

	return std::unique_ptr<IndexBuffer>(
		new IndexBuffer(
			std::move(Result.Buffer),
			std::move(Result.Memory),
			static_cast<uint32_t>(Indices.size())));
}

void IndexBuffer::Bind(const vk::raii::CommandBuffer& CommandBuffer, uint32_t binding) const
{
	CommandBuffer.bindIndexBuffer(*Buffer, 0, vk::IndexType::eUint32); // TODO: fix for now assuming single index buffer with no offsets for simplicity
}

Mesh::Mesh(	std::unique_ptr<VertexBuffer> VertexBuffer,
			std::unique_ptr<IndexBuffer> IndexBuffer)
	: VertexBufferPtr(std::move(VertexBuffer)),
	IndexBufferPtr(std::move(IndexBuffer))
{
}

std::unique_ptr<Mesh> Mesh::CreateFromMeshData(
	const vk::raii::Device& Device, 
	const vk::raii::PhysicalDevice& PhysicalDevice, 
	VulkanUploader* Uploader,
	const MeshData& MeshData)
{
	// Convert vertex array to raw bytes
	std::vector<uint8_t> RawVertexData(MeshData.GetVertices().size() * sizeof(Vertex));
	std::memcpy(RawVertexData.data(), MeshData.GetVertices().data(), RawVertexData.size());

	auto VB = VertexBuffer::Create(Device, PhysicalDevice, Uploader, RawVertexData, sizeof(Vertex));
	auto IB = IndexBuffer::Create(Device, PhysicalDevice, Uploader, MeshData.GetIndices());

	if (!VB || !IB) return nullptr;

	return std::unique_ptr<Mesh>(new Mesh(std::move(VB), std::move(IB)));
}

std::unique_ptr<Mesh> Mesh::Create(std::unique_ptr<VertexBuffer> VB,
								   std::unique_ptr<IndexBuffer> IB)
{
	return std::unique_ptr<Mesh>(new Mesh(std::move(VB), std::move(IB)));
}