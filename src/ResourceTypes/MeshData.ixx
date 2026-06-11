module;

#include <string>
#include <vector>
#include <glm/glm.hpp>

export module MeshData;

import ResourceBase;
import Geometry;

export class MeshData : public ResourceBase
{
public:
	explicit MeshData(const std::string& id) : ResourceBase(id) {}

	// Programmatic constructor — data provided directly, no file needed
	MeshData(
		const std::string& ID,
		std::vector<Vertex>   InVertices,
		std::vector<uint32_t> InIndices)
		: ResourceBase(ID)
		, Vertices(std::move(InVertices))
		, Indices(std::move(InIndices))
	{
		MeshBoundingBox = ComputeBoundingBox(Vertices);
	}

	const std::vector<Vertex>& GetVertices() const { return Vertices; }
	const std::vector<uint32_t>& GetIndices() const { return Indices; }
	const BoundingBox& GetBoundingBox() const { return MeshBoundingBox; }

protected:
	bool LoadResource(const std::string& FilePath) override;
	void UnloadResource() override;

private:
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;
	BoundingBox MeshBoundingBox;
};