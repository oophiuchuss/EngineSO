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
        glm::vec3 Corners[8] = {
           { Min.x, Min.y, Min.z },
           { Max.x, Min.y, Min.z },
           { Min.x, Max.y, Min.z },
           { Max.x, Max.y, Min.z },
           { Min.x, Min.y, Max.z },
           { Max.x, Min.y, Max.z },
           { Min.x, Max.y, Max.z },
           { Max.x, Max.y, Max.z },
        };

        Min = glm::vec3(FLT_MAX);
        Max = glm::vec3(-FLT_MAX);

        for (auto& Corner : Corners)
        {
            glm::vec3 Transformed = glm::vec3(TransformMatrix * glm::vec4(Corner, 1.0f));
            Min = glm::min(Min, Transformed);
            Max = glm::max(Max, Transformed);
        }
	}

    glm::vec3 Min;
    glm::vec3 Max;
};

export struct Vertex
{
	glm::vec3 Position;
	glm::vec2 UV = glm::vec2(0.0f);
	glm::vec3 Normal = glm::vec3(0.0f, 0.0f, 0.0f);
};