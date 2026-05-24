module;

#include <glm/ext/matrix_float4x4.hpp>

export module MeshComponent;

import Component;

import Mesh;


// TODO: implement the Material and Bounding Box classes
export class Material
{

};

export class BoundingBox
{
public:
	inline void Transform(const glm::mat4& TransformMatrix)
	{
	};
};

export class MeshComponent : public ComponentBase
{
public:
	MeshComponent(Mesh* Mesh, Material* Material) : MeshData(Mesh), MaterialData(Material) {}

	void SetMesh(Mesh* InMesh) { MeshData = InMesh; }
	void SetMaterial(Material* InMaterial) { MaterialData = InMaterial; }

	Mesh* GetMesh() const { return MeshData; }
	Material* GetMaterial() const { return MaterialData; }

	void Render() override;

	inline BoundingBox GetBoundingBox() const
	{
		//TODO: Implement the actual bounding box calculation based on the mesh data
		return BoundingBox();
	}

private:
	Mesh* MeshData = nullptr;
	Material* MaterialData = nullptr;
};
