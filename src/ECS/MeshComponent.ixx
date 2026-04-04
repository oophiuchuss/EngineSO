export module MeshComponent;

import Component;

import Mesh;

export class Material
{

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

private:
	Mesh* MeshData = nullptr;
	Material* MaterialData = nullptr;
};
