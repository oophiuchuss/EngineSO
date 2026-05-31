module;

#include <optional>
#include <string>
#include <vulkan/vulkan_raii.hpp>

export module Mesh;

import MeshData;

// TODO: maybe move the vertex buffer and index buffer management into a separate class or utility to keep the Mesh class focused on higher-level mesh management and resource loading logic
export class VertexBuffer
{
public:
	VertexBuffer() = default;
	~VertexBuffer() = default;

	// Non-copyable and movable semantics to manage Vulkan resources safely
	VertexBuffer(const VertexBuffer&) = delete;					// Prevent copying to avoid double-free issues with Vulkan resources
	VertexBuffer& operator=(const VertexBuffer&) = delete;		// Prevent copy assignment for the same reason
	VertexBuffer(VertexBuffer&&) noexcept = default;			// Allow move construction to transfer ownership of Vulkan resources without copying
	VertexBuffer& operator=(VertexBuffer&&) noexcept = default;	// Allow move assignment for the same reason

	// Factory method to create a vertex buffer from raw vertex data, handling Vulkan buffer creation and memory management internally
	static std::unique_ptr<VertexBuffer> Create(
		const vk::raii::Device& Device,
		const vk::raii::PhysicalDevice& PhysicalDevice,
		const std::vector<uint8_t>& Data,
		uint8_t Stride);

	// Method to bind the vertex buffer to a command buffer for rendering, specifying the binding point (default is 0)
	void Bind(const vk::raii::CommandBuffer& CommandBuffer, uint32_t binding = 0) const;

	inline uint32_t GetVertexCount() const { return VertexCount; }
	inline uint32_t GetStride() const { return Stride; }

private:
	// Private constructor to enforce creation through the factory method, ensuring proper Vulkan resource management
	VertexBuffer(
		vk::raii::Buffer&& Buffer,
		vk::raii::DeviceMemory&& DeviceMemory,
		uint32_t VertexCount,
		uint8_t Stride);

	vk::raii::Buffer Buffer = nullptr;				// Vulkan buffer handle for vertex data
	vk::raii::DeviceMemory BufferMemory = nullptr;	// Memory allocated for the vertex buffer
	uint32_t VertexCount = 0;						// Number of vertices stored in the vertex buffer
	uint8_t Stride = 0;								// Size of each vertex in bytes (e.g., position + normal + UVs)
};

export class IndexBuffer
{
public:
	IndexBuffer() = default;
	~IndexBuffer() = default;

	// Non-copyable and movable semantics to manage Vulkan resources safely
	IndexBuffer(const IndexBuffer&) = delete;					// Prevent copying to avoid double-free issues with Vulkan resources
	IndexBuffer& operator=(const IndexBuffer&) = delete;		// Prevent copy assignment for the same reason
	IndexBuffer(IndexBuffer&&) noexcept = default;				// Allow move construction to transfer ownership of Vulkan resources without copying
	IndexBuffer& operator=(IndexBuffer&&) noexcept = default;	// Allow move assignment for the same reason

	// Factory method to create an index buffer from raw index data, handling Vulkan buffer creation and memory management internally
	static std::unique_ptr<IndexBuffer> Create(
		const vk::raii::Device& Device,
		const vk::raii::PhysicalDevice& PhysicalDevice,
		const std::vector<uint32_t>& Indices);

	// Method to bind the index buffer to a command buffer for rendering, specifying the binding point (default is 0)
	void Bind(const vk::raii::CommandBuffer& CommandBuffer, uint32_t binding = 0) const;

	inline uint32_t GetIndexCount() const { return IndexCount; }

private:
	// Private constructor to enforce creation through the factory method, ensuring proper Vulkan resource management
	IndexBuffer(
		vk::raii::Buffer&& Buffer,
		vk::raii::DeviceMemory&& DeviceMemory,
		uint32_t IndexCount);

	vk::raii::Buffer Buffer = nullptr;				// Vulkan buffer handle for index data
	vk::raii::DeviceMemory BufferMemory = nullptr;	// Memory allocated for the index buffer
	uint32_t IndexCount = 0;						// Number of indices stored in the index buffer
};

export class Mesh
{
public:
	Mesh() = default;
	~Mesh() = default;

	// Non-copyable and movable semantics to manage Vulkan resources safely
	Mesh(const Mesh&) = delete;						// Prevent copying to avoid double-free issues with Vulkan resources
	Mesh& operator=(const Mesh&) = delete;			// Prevent copy assignment for the same reason
	Mesh(Mesh&&) noexcept = default;				// Allow move construction to transfer ownership of Vulkan resources without copying
	Mesh& operator=(Mesh&&) noexcept = default;		// Allow move assignment for the same reason

	static std::unique_ptr<Mesh> CreateFromMeshData(
		const vk::raii::Device& Device,
		const vk::raii::PhysicalDevice& PhysicalDevice,
		const MeshData& MeshData);
	
	static std::unique_ptr<Mesh> Create(
		std::unique_ptr<VertexBuffer> VB,
		std::unique_ptr<IndexBuffer> IB);
	
	inline VertexBuffer* GetVertexBuffer() const { return VertexBufferPtr.get(); }
	inline IndexBuffer* GetIndexBuffer() const { return IndexBufferPtr.get(); }

private:
	// Private constructor to enforce creation through the factory method, ensuring proper Vulkan resource management
	Mesh(std::unique_ptr<VertexBuffer> VertexBuffer,
		 std::unique_ptr<IndexBuffer> IndexBuffer);

	std::unique_ptr<VertexBuffer> VertexBufferPtr;	// Unique pointer to manage the vertex buffer resource
	std::unique_ptr<IndexBuffer> IndexBufferPtr;	// Unique pointer to manage the index buffer resource
};
