module;

#include <string>
#include <vector>
#include <glm/glm.hpp>

module MeshData;

import Geometry;

bool MeshData::Reprocess(const ReprocessOptions& Options)
{
	const auto* MeshOpts = dynamic_cast<const MeshReprocessOptions*>(&Options);
	if (!MeshOpts || MeshOpts->bFullReconstruct || !MeshOpts->bRegenerateNormals)
	{
		return false;
	}

	std::vector<glm::vec3> Positions;
	Positions.reserve(Vertices.size());

	for (const Vertex& V : Vertices)
	{
		Positions.push_back(V.Position);
	}

	std::vector<glm::vec3> NewNormals = GenerateNormals(MeshOpts->Technique, Positions, Indices);

	for (size_t i = 0; i < Vertices.size(); ++i)
	{
		Vertices[i].Normal = NewNormals[i];
	}

	return true;
}

bool MeshData::LoadResource(const std::string& FilePath)
{
	// Already populated via programmatic constructor
	if (!Vertices.empty()) return true;

	// TODO: implement actual mesh loading from file (e.g. using tinyobjloader for .obj files, or a custom binary format for better performance)
	// For now, we will just create a simple triangle mesh for testing purposes
	Vertices = {
		{ glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) },
		{ glm::vec3(0.5f, -0.5f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) },
		{ glm::vec3(0.0f,  0.5f, 0.0f), glm::vec2(0.5f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f) },
	};
	Indices = { 0, 1, 2 };

	MeshBoundingBox = ComputeBoundingBox(Vertices);
	
	return true;
}

void MeshData::UnloadResource()
{
	Vertices.clear();
	Indices.clear();
	MeshBoundingBox = {};
}