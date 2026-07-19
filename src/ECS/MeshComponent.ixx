module;

#include <memory>
#include <string>

export module MeshComponent;

import Component;
import ResourceHandle;
import MeshData;
import Material;


export class MeshComponent : public ComponentBase
{
public:
	explicit MeshComponent(
		ResourceHandle<MeshData> InMeshHandle,
		ResourceHandle<Material> InMaterialHandle) :
		MeshHandle(InMeshHandle),
		MaterialHandle(InMaterialHandle) 
	{
	}

	const MeshData* GetMeshData() const { return MeshHandle.Get(); }
	const Material* GetMaterial() const { return MaterialHandle.Get(); }

	void SetMaterial(ResourceHandle<Material> InMaterialHandle) { MaterialHandle = InMaterialHandle; }
private:
	ResourceHandle<MeshData> MeshHandle;
	ResourceHandle<Material> MaterialHandle;
};
