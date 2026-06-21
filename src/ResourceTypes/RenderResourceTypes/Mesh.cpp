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

std::unique_ptr<VertexBuffer> VertexBuffer::CreateFromUploadResult(vk::raii::Buffer&& Buffer, vk::raii::DeviceMemory&& Memory, uint32_t VertexCount, uint8_t Stride)
{
	return std::unique_ptr<VertexBuffer>(new VertexBuffer(std::move(Buffer), std::move(Memory), VertexCount, Stride));
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

std::unique_ptr<IndexBuffer> IndexBuffer::CreateFromUploadResult(vk::raii::Buffer&& Buffer, vk::raii::DeviceMemory&& Memory, uint32_t IndexCount)
{
	return std::unique_ptr<IndexBuffer>(new IndexBuffer(std::move(Buffer), std::move(Memory), IndexCount));
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

std::vector<std::unique_ptr<Mesh>> Mesh::CreateFromMeshDataBatch(const vk::raii::Device& Device, const vk::raii::PhysicalDevice& PhysicalDevice, VulkanUploader& Uploader, const std::vector<const MeshData*>& DataList)
{
	std::vector<std::unique_ptr<Mesh>> Meshes;
	if (DataList.empty()) return Meshes;

	// Collect all raw vertex and index data into persistent vectors
	std::vector<std::vector<uint8_t>>  RawVertexDataList;
	std::vector<std::vector<uint32_t>> RawIndexDataList;
	RawVertexDataList.reserve(DataList.size());
	RawIndexDataList.reserve(DataList.size());

	for (const auto* MD : DataList)
	{
		const auto& Vertices = MD->GetVertices();
		const auto& Indices = MD->GetIndices();

		std::vector<uint8_t> RawV(Vertices.size() * sizeof(Vertex));
		std::memcpy(RawV.data(), Vertices.data(), RawV.size());
		RawVertexDataList.push_back(std::move(RawV));

		RawIndexDataList.push_back(Indices);   // copy
	}

	std::vector<VulkanUploader::BufferUploadInfo> VertexUploads;
	std::vector<VulkanUploader::BufferUploadInfo> IndexUploads;
	VertexUploads.reserve(DataList.size());
	IndexUploads.reserve(DataList.size());

	for (size_t i = 0; i < DataList.size(); ++i)
	{
		VertexUploads.push_back({
			RawVertexDataList[i].data(),
			RawVertexDataList[i].size(),
			vk::BufferUsageFlagBits::eVertexBuffer
			});

		IndexUploads.push_back({
			RawIndexDataList[i].data(),
			sizeof(uint32_t) * RawIndexDataList[i].size(),
			vk::BufferUsageFlagBits::eIndexBuffer
			});
	}

	auto VertexResults = Uploader.UploadBufferBatch(VertexUploads);
	auto IndexResults = Uploader.UploadBufferBatch(IndexUploads);

	// Assemble Meshes from the uploaded buffers
	for (size_t i = 0; i < DataList.size(); ++i)
	{
		auto& VRes = VertexResults[i];
		auto& IRes = IndexResults[i];

		auto VB = VertexBuffer::CreateFromUploadResult(
			std::move(VRes.Buffer), std::move(VRes.Memory),
			static_cast<uint32_t>(DataList[i]->GetVertices().size()),
			sizeof(Vertex));

		auto IB = IndexBuffer::CreateFromUploadResult(
			std::move(IRes.Buffer), std::move(IRes.Memory),
			static_cast<uint32_t>(DataList[i]->GetIndices().size()));

		Meshes.push_back(std::unique_ptr<Mesh>(new Mesh(std::move(VB), std::move(IB))));
	}

	return Meshes;
}
