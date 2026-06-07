module;

#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

module RenderResourceCache;

import Mesh;
import Shader;
import MeshData;
import ShaderData;

Mesh* RenderResourceCache::GetOrUploadMesh(const std::string& ID, const MeshData& Data)
{
    auto it = MeshCache.find(ID);
    if (it != MeshCache.end())
        return it->second.get();

    auto NewMesh = Mesh::CreateFromMeshData(Device, PhysicalDevice, UploaderPtr, Data);
    if (!NewMesh) return nullptr;

    Mesh* Ptr = NewMesh.get();
    MeshCache[ID] = std::move(NewMesh);
    return Ptr;
}

Shader* RenderResourceCache::GetOrCompileShader(const std::string& ID, const ShaderData& Data)
{
    auto it = ShaderCache.find(ID);
    if (it != ShaderCache.end())
        return it->second.get();

    auto NewShader = Shader::CreateFromBytecode(Device, Data.VertexBytecode, Data.FragmentBytecode);
    if (!NewShader) return nullptr;

    Shader* Ptr = NewShader.get();
    ShaderCache[ID] = std::move(NewShader);
    return Ptr;
}

void RenderResourceCache::Evict(const std::string& ID)
{
    MeshCache.erase(ID);
    ShaderCache.erase(ID);
}

void RenderResourceCache::EvictAll()
{
    MeshCache.clear();
    ShaderCache.clear();
}