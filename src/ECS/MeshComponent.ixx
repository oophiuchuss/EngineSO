module;

#include <glm/glm.hpp>
#include <memory>
#include <string>

export module MeshComponent;

import Component;
import ResourceHandle;
import MeshData;
import Material;
import MaterialProperties; 


export class MeshComponent : public ComponentBase
{
public:
	explicit MeshComponent(
		ResourceHandle<MeshData> InMeshHandle,
		ResourceHandle<Material> InMaterialHandle) :
		MeshHandle(InMeshHandle),
		MaterialHandle(InMaterialHandle) 
	{
		if (const Material* Mat = MaterialHandle.Get())
		{
			MaterialInstanceData = Mat->GetMaterialProperties();
		}
	}

	const MeshData* GetMeshData() const { return MeshHandle.Get(); }
	const Material* GetMaterial() const { return MaterialHandle.Get(); }

	// Per-instance overrides — shared material is never touched
	void SetAlbedo(glm::vec4 Color) { MaterialInstanceData.AlbedoColor = Color; }
	void SetMetallic(float Value) { MaterialInstanceData.Metallic = Value; }
	void SetRoughness(float Value) { MaterialInstanceData.Roughness = Value; }
	void SetEmissiveStrength(float Value) { MaterialInstanceData.EmissiveStrength = Value; }

	// Returns per-instance data — initialized from material, overridable per entity
	const MaterialProperties& GetEffectiveMaterialProperties() const { return MaterialInstanceData; }

private:
	ResourceHandle<MeshData> MeshHandle;
	ResourceHandle<Material> MaterialHandle;
	MaterialProperties MaterialInstanceData;  // per-entity copy, starts from material defaults
};
