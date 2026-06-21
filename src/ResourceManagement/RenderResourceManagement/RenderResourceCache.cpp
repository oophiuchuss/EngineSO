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
    auto NewTexture = Texture::CreateFromTextureData(Device, *UploaderPtr, Data);
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

void RenderResourceCache::GetOrUploadMeshBatch(const std::vector<std::string>& IDs, const std::vector<const MeshData*>& DataList)
{
    if (IDs.empty() || IDs.size() != DataList.size())
    {
        return;
    }

    std::vector<const MeshData*> NewData;
    std::vector<std::string> NewIDs;
    for (size_t i = 0; i < IDs.size(); ++i)
    {
        if (!IsMeshCached(IDs[i]))
        {
            NewIDs.push_back(IDs[i]);
            NewData.push_back(DataList[i]);
        }
    }

    if (NewData.empty())
    {
        return;
    }

    auto NewMeshes = Mesh::CreateFromMeshDataBatch(Device, PhysicalDevice, *UploaderPtr, NewData);

    for (size_t i = 0; i < NewMeshes.size(); ++i)
    {
        MeshCache[NewIDs[i]] = std::move(NewMeshes[i]);
    }
}

std::vector<int> RenderResourceCache::GetOrUploadTextureBatch(const std::vector<std::string>& IDs, const std::vector<const TextureData*>& DataList)
{
    if (IDs.size() != DataList.size())
    {
        throw std::invalid_argument("GetOrUploadTextureBatch: IDs and DataList size mismatch");
    }

    std::vector<int> ResultSlots(IDs.size(), -1);

    // Figure out which ones are already cached, and which need uploading
    std::vector<size_t> PendingIndices;             // index into IDs/DataList
    std::vector<const TextureData*> PendingData;

    for (size_t i = 0; i < IDs.size(); ++i)
    {
        auto It = TextureSlotMap.find(IDs[i]);
        if (It != TextureSlotMap.end())
        {
            ResultSlots[i] = It->second; // already uploaded — reuse slot
        }
        else
        {
            PendingIndices.push_back(i);
            PendingData.push_back(DataList[i]);
        }
    }

    if (PendingData.empty())
    {
        return ResultSlots; // everything was already cached
    }

    // Single batched GPU upload for everything not yet cached
    std::vector<std::unique_ptr<Texture>> NewTextures = Texture::CreateBatchFromTextureData(Device, *UploaderPtr, PendingData);

    if (NewTextures.size() != PendingData.size())
    {
        throw std::runtime_error("GetOrUploadTextureBatch: batch upload returned mismatched count");
    }

    // Allocate slots and write descriptors for each newly uploaded texture
    for (size_t i = 0; i < NewTextures.size(); ++i)
    {
        size_t OriginalIndex = PendingIndices[i];
        const std::string& ID = IDs[OriginalIndex];

        int Slot = DescriptorHeapPtr->AllocateSlot();
        if (Slot < 0)
        {
            ResultSlots[OriginalIndex] = -1; // out of slots — caller falls back to default
            continue;
        }

        DescriptorHeapPtr->WriteSlot(Slot, NewTextures[i]->GetImageView(), DataList[OriginalIndex]->GetSamplerDesc());

        TextureCache[ID] = std::move(NewTextures[i]);
        TextureSlotMap[ID] = Slot;
        ResultSlots[OriginalIndex] = Slot;
    }

    return ResultSlots;
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