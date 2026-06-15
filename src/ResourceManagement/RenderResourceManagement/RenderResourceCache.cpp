module;

#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

module RenderResourceCache;

import Mesh;
import Shader;
import Texture;
import MeshData;
import ShaderData;
import TextureData;

Mesh* RenderResourceCache::GetOrUploadMesh(const std::string& ID, const MeshData& Data)
{
    auto it = MeshCache.find(ID);
    if (it != MeshCache.end())
    {
        return it->second.get();
    }

    auto NewMesh = Mesh::CreateFromMeshData(Device, PhysicalDevice, UploaderPtr, Data);
    if (!NewMesh)
    {
        return nullptr;
    }

    Mesh* Ptr = NewMesh.get();
    MeshCache[ID] = std::move(NewMesh);
    return Ptr;
}

Shader* RenderResourceCache::GetOrCompileShader(const std::string& ID, const ShaderData& Data)
{
    auto it = ShaderCache.find(ID);
    if (it != ShaderCache.end())
    {
        return it->second.get();
    }

    auto NewShader = Shader::CreateFromBytecode(Device, Data.VertexBytecode, Data.FragmentBytecode);
    if (!NewShader)
    {
        return nullptr;
    }

    Shader* Ptr = NewShader.get();
    ShaderCache[ID] = std::move(NewShader);
    return Ptr;
}

int RenderResourceCache::GetOrUploadTexture(const std::string& ID, const TextureData& Data)
{
    // If already cached, return the existing slot index
    auto It = TextureSlotMap.find(ID);
    if (It != TextureSlotMap.end())
    {
        return It->second;
    }

    // Upload GPU image
    auto NewTexture = Texture::CreateFromTextureData(Device, PhysicalDevice, *UploaderPtr, Data);
    if (!NewTexture)
    {
        return -1;
    }

    // Allocate a slot in the descriptor heap
    int Slot = DescriptorHeapPtr->AllocateSlot();
    if (Slot < 0)
    {
        return -1;
    }

    // Write the descriptor with the sampler from the texture data
    DescriptorHeapPtr->WriteSlot(Slot, NewTexture->GetImageView(), Data.GetSamplerDesc());

    // Cache the texture and its slot index
    TextureCache[ID] = std::move(NewTexture);
    TextureSlotMap[ID] = Slot;

    return Slot;
}

void RenderResourceCache::Evict(const std::string& ID)
{
    MeshCache.erase(ID);
    ShaderCache.erase(ID);

    auto TexIt = TextureCache.find(ID);
    if (TexIt != TextureCache.end())
    {
        auto SlotIt = TextureSlotMap.find(ID);
        if (SlotIt != TextureSlotMap.end())
        {
            DescriptorHeapPtr->FreeSlot(SlotIt->second);
            TextureSlotMap.erase(SlotIt);
        }
        TextureCache.erase(TexIt);
    }
}

void RenderResourceCache::EvictAll()
{
    MeshCache.clear();
    ShaderCache.clear();

    for (auto& [ID, Slot] : TextureSlotMap)
    {
        DescriptorHeapPtr->FreeSlot(Slot);
    }

    TextureCache.clear();
    TextureSlotMap.clear();
}