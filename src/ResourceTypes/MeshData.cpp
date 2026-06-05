module;

#include <string>
#include <vector>
#include <glm/glm.hpp>

module MeshData;

import Geometry;

bool MeshData::LoadResource(const std::string& FilePath)
{
	// TODO: implement actual mesh loading from file (e.g. using tinyobjloader for .obj files, or a custom binary format for better performance)
	// For now, we will just create a simple triangle mesh for testing purposes
	Vertices = {
		{ glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) },
		{ glm::vec3(0.5f, -0.5f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) },
		{ glm::vec3(0.0f,  0.5f, 0.0f), glm::vec2(0.5f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f) },
	};
	Indices = { 0, 1, 2 };

	MeshBoundingBox = BoundingBox();
	MeshBoundingBox.Min = glm::vec3(-0.5f, -0.5f, 0.0f);
	MeshBoundingBox.Max = glm::vec3(0.5f, 0.5f, 0.0f);
	
	return true;
}

void MeshData::UnloadResource()
{
	Vertices.clear();
	Indices.clear();
	MeshBoundingBox = {};
}