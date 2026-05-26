module;

#include <glm/glm.hpp>

export module Geometry;

export class BoundingBox
{
public:
    BoundingBox() {};
	BoundingBox(const glm::vec3& InMin, const glm::vec3& InMax) : Min(InMin), Max(InMax) {}

	void Transform(const glm::mat4& TransformMatrix)
	{
		// TODO: potentially improve to 8 corners
		
		// Apply matrix to both corners, then recompute min and max
		glm::vec4 Corners[2] = {
			TransformMatrix * glm::vec4(Min, 1.0f),
			TransformMatrix * glm::vec4(Max, 1.0f)
		};

		Min = glm::min(glm::vec3(Corners[0]), glm::vec3(Corners[1]));
		Max = glm::max(glm::vec3(Corners[0]), glm::vec3(Corners[1]));
	}

    glm::vec3 Min;
    glm::vec3 Max;
};

export struct Vertex
{
	glm::vec3 Position;
	// TODO: add more vertex attributes like normals, UVs, etc.
};