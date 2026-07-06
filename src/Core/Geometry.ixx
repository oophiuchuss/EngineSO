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

export std::vector<glm::vec3> GenerateSmoothNormals(const std::vector<glm::vec3>& Positions, const std::vector<uint32_t>& Indices)
{
    std::vector<glm::vec3> Normals(Positions.size(), glm::vec3(0.0f));

    // Accumulate angle-weighted face normals into every vertex they touch.
    for (size_t i = 0; i + 2 < Indices.size(); i += 3)
    {
        uint32_t IA = Indices[i];
        uint32_t IB = Indices[i + 1];
        uint32_t IC = Indices[i + 2];

        const glm::vec3& A = Positions[IA];
        const glm::vec3& B = Positions[IB];
        const glm::vec3& C = Positions[IC];

        glm::vec3 EdgeAB = B - A;
        glm::vec3 EdgeAC = C - A;
        glm::vec3 FaceNormal = glm::cross(EdgeAB, EdgeAC);

        float FaceLength = glm::length(FaceNormal);
        if (FaceLength < 1e-8f)
        {
            continue; // degenerate triangle (zero area) — contributes nothing
        }
        glm::vec3 UnitFaceNormal = FaceNormal / FaceLength;

        // Angle-weighted: weight this face's contribution to each vertex by
        // the angle it subtends AT that specific vertex — makes the result
        // independent of how finely the surface happens to be tessellated.
        auto AngleAt = [](const glm::vec3& P, const glm::vec3& P1, const glm::vec3& P2) -> float
            {
                glm::vec3 V1 = glm::normalize(P1 - P);
                glm::vec3 V2 = glm::normalize(P2 - P);
                float CosAngle = glm::clamp(glm::dot(V1, V2), -1.0f, 1.0f);
                return glm::acos(CosAngle);
            };

        Normals[IA] += UnitFaceNormal * AngleAt(A, B, C);
        Normals[IB] += UnitFaceNormal * AngleAt(B, C, A);
        Normals[IC] += UnitFaceNormal * AngleAt(C, A, B);
    }

    for (glm::vec3& N : Normals)
    {
        float Len = glm::length(N);
        N = (Len > 1e-8f) ? (N / Len) : glm::vec3(0.0f, 1.0f, 0.0f); // fallback for any unreferenced vertex
    }

    return Normals;
}