module;

#include <memory>
#include <string>

export module MeshComponent;

import Component;
import ResourceManager;
import MeshData;
import ShaderData;

// TODO: figure out where to put material
export class Material
{

};

export class MeshComponent : public ComponentBase
{
public:
	explicit MeshComponent(ResourceHandle<MeshData> InMeshHandle, ResourceHandle<ShaderData> InShaderHandle) : MeshHandle(InMeshHandle), ShaderHandle(InShaderHandle) {}

	const MeshData* GetMeshData() const { return MeshHandle.Get(); }
	const ShaderData* GetShaderData() const { return ShaderHandle.Get(); }

private:
	ResourceHandle<MeshData> MeshHandle;
	ResourceHandle<ShaderData> ShaderHandle;
};
