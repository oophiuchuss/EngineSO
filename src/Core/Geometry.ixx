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

export BoundingBox ComputeBoundingBox(const std::vector<Vertex>& Vertices)
{
    BoundingBox Bounds;
    Bounds.Min = glm::vec3(FLT_MAX);
    Bounds.Max = glm::vec3(-FLT_MAX);

    for (const auto& V : Vertices)
    {
        Bounds.Min = glm::min(Bounds.Min, V.Position);
        Bounds.Max = glm::max(Bounds.Max, V.Position);
    }

    return Bounds;
}

struct Vec3Hash
{
    size_t operator()(const glm::vec3& V) const
    {
        size_t H1 = std::hash<float>{}(V.x);
        size_t H2 = std::hash<float>{}(V.y);
        size_t H3 = std::hash<float>{}(V.z);
        return H1 ^ (H2 * 0x9e3779b9) ^ (H3 * 0x85ebca6b);
    }
};
struct Vec3Equal
{
    bool operator()(const glm::vec3& A, const glm::vec3& B) const { return A == B; }
};

export std::vector<glm::vec3> GenerateNormalsAreaWeighted(
    const std::vector<glm::vec3>& Positions,
    const std::vector<uint32_t>& Indices)
{
    std::vector<glm::vec3> Normals(Positions.size(), glm::vec3(0.0f));

    // Group indices sharing the same position — exporters commonly duplicate a
    // vertex (same position, different index) at UV seams or patch boundaries.
    // Accumulating by index alone fails to smooth across these, producing
    // visible facet boundaries exactly at those seams.
    std::unordered_map<glm::vec3, std::vector<uint32_t>, Vec3Hash, Vec3Equal> PositionToIndices;
    for (uint32_t i = 0; i < Positions.size(); ++i)
    {
        PositionToIndices[Positions[i]].push_back(i);
    }

    if (!Indices.empty())
    {
        for (size_t i = 0; i + 2 < Indices.size(); i += 3)
        {
            const uint32_t i0 = Indices[i + 0];
            const uint32_t i1 = Indices[i + 1];
            const uint32_t i2 = Indices[i + 2];

            const glm::vec3& p0 = Positions[i0];
            const glm::vec3& p1 = Positions[i1];
            const glm::vec3& p2 = Positions[i2];

            glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
            if (glm::dot(n, n) > 1e-12f)
            {
                for (uint32_t Idx : PositionToIndices[p0]) Normals[Idx] += n;
                for (uint32_t Idx : PositionToIndices[p1]) Normals[Idx] += n;
                for (uint32_t Idx : PositionToIndices[p2]) Normals[Idx] += n;
            }
        }
    }
    else
    {
        for (size_t i = 0; i + 2 < Positions.size(); i += 3)
        {
            const glm::vec3& p0 = Positions[i];
            const glm::vec3& p1 = Positions[i + 1];
            const glm::vec3& p2 = Positions[i + 2];

            glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
            if (glm::dot(n, n) > 1e-12f)
            {
                for (uint32_t Idx : PositionToIndices[p0]) Normals[Idx] += n;
                for (uint32_t Idx : PositionToIndices[p1]) Normals[Idx] += n;
                for (uint32_t Idx : PositionToIndices[p2]) Normals[Idx] += n;
            }
        }
    }

    for (glm::vec3& n : Normals)
    {
        n = (glm::dot(n, n) > 1e-12f) ? glm::normalize(n) : glm::vec3(0.0f, 1.0f, 0.0f);
    }

    return Normals;
}