module;

#include <glm/ext/matrix_float4x4.hpp>
#include <memory>

export module MeshComponent;

import Component;

import Mesh;
import Geometry;


// TODO: implement the Material and Bounding Box classes
export class Material
{
};

export class MeshComponent : public ComponentBase
{
public:
	explicit MeshComponent(std::shared_ptr<Mesh> Mesh = nullptr, Material* Material = nullptr) : MeshData(std::move(Mesh)), MaterialData(Material) {}

	void SetMesh(std::shared_ptr<Mesh> InMesh) { MeshData = InMesh; }
	void SetMaterial(Material* InMaterial) { MaterialData = InMaterial; }

	std::shared_ptr<Mesh> GetMesh() const { return MeshData; }
	Material* GetMaterial() const { return MaterialData; }

	void Render() override;

	BoundingBox GetBoundingBox() const;

private:
	std::shared_ptr<Mesh> MeshData = nullptr;
	Material* MaterialData = nullptr; // TODO: replace with smart pointer and implement material management
};
