module;

#include <string>
#include <vector>
#include <glm/glm.hpp>

export module MeshData;

import ResourceBase;
import ReprocessOptions;
import Geometry;

export struct MeshReprocessOptions : public ReprocessOptions
{
	bool bRegenerateNormals = true; // false = no-op, Reprocess() reports nothing changed
	NormalGenerationTechnique Technique = NormalGenerationTechnique::AreaWeighted;
};

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

	bool Reprocess(const ReprocessOptions& Options) override;

	const std::vector<Vertex>& GetVertices() const { return Vertices; }
	const std::vector<uint32_t>& GetIndices() const { return Indices; }
	const BoundingBox& GetBoundingBox() const { return MeshBoundingBox; }

	static std::string_view AssetFolder() { return "meshes"; }
	static std::string_view FileExtension() { return ".obj"; }

protected:
	bool LoadResource(const std::string& FilePath) override;
	void UnloadResource() override;

private:
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;
	BoundingBox MeshBoundingBox;
};