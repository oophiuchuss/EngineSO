module;

#include <string>

module Mesh;

bool Mesh::LoadResource()
{
	// Construct the file path for the mesh based on the resource ID
	std::string FilePath = "assets/meshes/" + GetResourceID() + ".gltf";
	
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
}
