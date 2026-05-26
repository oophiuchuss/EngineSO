module;

#include <optional>
#include <string>
#include <vulkan/vulkan_raii.hpp>

export module Mesh;

//import ResourceBase;
import Geometry;

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

	// Factory for test triangle
	static std::unique_ptr<Mesh> CreateTestTriangle(
		const vk::raii::Device& Device,
		const vk::raii::PhysicalDevice& PhysicalDevice);

	inline VertexBuffer* GetVertexBuffer() const { return VertexBufferPtr.get(); }
	inline IndexBuffer* GetIndexBuffer() const { return IndexBufferPtr.get(); }
	inline const BoundingBox& GetBoundingBox() const { return Bounds; }

private:
	// Private constructor to enforce creation through the factory method, ensuring proper Vulkan resource management
	Mesh(std::unique_ptr<VertexBuffer> VertexBuffer,
		 std::unique_ptr<IndexBuffer> IndexBuffer,
		 const BoundingBox& Bounds);

	std::unique_ptr<VertexBuffer> VertexBufferPtr;	// Unique pointer to manage the vertex buffer resource
	std::unique_ptr<IndexBuffer> IndexBufferPtr;	// Unique pointer to manage the index buffer resource
	BoundingBox Bounds;								// Axis-aligned bounding box for the mesh, used for culling and collision detection
};

/*export class Mesh : public ResourceBase
{
public:
	explicit Mesh(const std::string& id) : ResourceBase(id) {}
	
	~Mesh()
	{
		Unload(); // Ensure resources are released when the mesh is destroyed
	}

	// Accessors for Vulkan buffer handles and vertex/index counts, returning default-constructed handles if not loaded
	vk::Buffer GetVertexBuffer() const { return VertexBuffer ? **VertexBuffer : vk::Buffer{}; }
	vk::Buffer GetIndexBuffer() const { return IndexBuffer ? **IndexBuffer : vk::Buffer{}; }
	uint32_t GetVertexCount() const { return VertexCount; }
	uint32_t GetIndexCount() const { return IndexCount; }

protected:
	bool LoadResource(const std::string& FilePath) override;
	void UnloadResource() override;

private:
	// TODO: Implement actual mesh loading, Vulkan buffer creation, and memory management logic
	// Placeholder functions for mesh loading and Vulkan resource creation
	bool LoadMeshData(const std::string& FilePath, std::vector<Vertex>& OutVertices, std::vector<uint32_t>& OutIndices) { return false; }
	void CreateVertexBuffer(const std::vector<Vertex>& Vertices) {}
	void CreateIndexBuffer(const std::vector<uint32_t>& Indices) {}
	vk::Device GetDevice() const { return vk::Device(); }

	// Vertex data management - stores per-vertex attributes like position, normal, UVs, etc.
	std::optional<vk::raii::Buffer> VertexBuffer;				// Vulkan buffer handle for vertex data
	std::optional<vk::raii::DeviceMemory> VertexBufferMemory;	// Memory allocated for the vertex buffer
	vk::DeviceSize VertexBufferOffset;							// Offset within the allocated memory where the vertex data starts
	uint32_t VertexCount = 0;									// Number of vertices stored in the vertex buffer
	
	// Index data management - stores indices for indexed drawing, referencing vertices in the vertex buffer
	std::optional<vk::raii::Buffer> IndexBuffer;				// Vulkan buffer handle for index data
	std::optional<vk::raii::DeviceMemory> IndexBufferMemory;	// Memory allocated for the index buffer
	vk::DeviceSize IndexBufferOffset;							// Offset within the allocated memory where the index data starts
	uint32_t IndexCount = 0;									// Number of indices stored in the index buffer
};*/
