module;

#include <string>
#include <vector>
#include <array>
#include <vulkan/vulkan_raii.hpp>

module RenderResourceCache;

import Mesh;
import Shader;
import Texture;
import MeshData;
import ShaderData;
import TextureData;

namespace
{
    vk::Format GetBasisTargetFormat(BasisTranscodeTarget Target, TextureColorSpace ColorSpace)
    {
        const bool bSRGB = ColorSpace == TextureColorSpace::SRGB;

        switch (Target)
        {
        case BasisTranscodeTarget::BC7RGBA:
            return bSRGB ? vk::Format::eBc7SrgbBlock : vk::Format::eBc7UnormBlock;

        case BasisTranscodeTarget::ASTC4x4RGBA:
            return bSRGB ? vk::Format::eAstc4x4SrgbBlock : vk::Format::eAstc4x4UnormBlock;

        case BasisTranscodeTarget::ETC2RGBA:
            return bSRGB ? vk::Format::eEtc2R8G8B8A8SrgbBlock : vk::Format::eEtc2R8G8B8A8UnormBlock;

        case BasisTranscodeTarget::BC3RGBA:
            return bSRGB ? vk::Format::eBc3SrgbBlock : vk::Format::eBc3UnormBlock;

        case BasisTranscodeTarget::RGBA32:
            return bSRGB ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

        default:
            return vk::Format::eUndefined;
        }
    }
}

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

    std::unique_ptr<Shader> NewShader;
    
    if (Data.GetProgramType() == ShaderProgramType::Compute)
    {
        NewShader = Shader::CreateComputeFromBytecode(Device, Data.ComputeBytecode);
    }
	else if (Data.GetProgramType() == ShaderProgramType::Graphics)
    {
        NewShader = Shader::CreateFromBytecode(Device, Data.VertexBytecode, Data.FragmentBytecode);
    }
    else
    {
		throw std::runtime_error("GetOrCompileShader: Unsupported shader program type");
    }
    
    if (!NewShader)
    {
        return nullptr;
    }

    Shader* Ptr = NewShader.get();
    ShaderCache[ID] = std::move(NewShader);
    return Ptr;
}

int RenderResourceCache::GetOrUploadTexture(const std::string& ID, TextureData& Data)
{
    // If already cached, return the existing slot index
    auto It = TextureSlotMap.find(ID);
    if (It != TextureSlotMap.end())
    {
        return It->second;
    }

    PrepareTextureForUpload(Data);

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

std::vector<int> RenderResourceCache::GetOrUploadTextureBatch(const std::vector<std::string>& IDs, const std::vector<TextureData*>& DataList)
{
    if (IDs.size() != DataList.size())
    {
        throw std::invalid_argument("GetOrUploadTextureBatch: IDs and DataList size mismatch");
    }

    std::vector<int> ResultSlots(IDs.size(), -1);

    // Figure out which ones are already cached, and which need uploading
    std::vector<size_t> PendingIndices;             // index into IDs/DataList
    std::vector<TextureData*> PendingData;

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

    for (TextureData* Data : PendingData)
    {
        if (!Data)
        {
            throw std::runtime_error("Texture batch contains null data");
        }

        PrepareTextureForUpload(*Data);
    }

    std::vector<const TextureData*> PreparedData(PendingData.begin(), PendingData.end());

    // Single batched GPU upload for everything not yet cached
    std::vector<std::unique_ptr<Texture>> NewTextures = Texture::CreateBatchFromTextureData(Device, *UploaderPtr, PreparedData);

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

void RenderResourceCache::PrepareTextureForUpload(TextureData& Data)
{
    if (Data.GetSourceEncoding() == TextureSourceEncoding::Direct)
    {
        if (!Data.IsUploadReady())
        {
            throw std::runtime_error("Direct texture has no upload payload");
        }

        return;
    }

    const BasisTranscodeTarget Target = SelectBasisTranscodeTarget(Data);

    if (!Data.IsPreparedFor(Target) && !Data.PrepareBasisPayload(Target))
    {
        throw std::runtime_error("Basis texture transcoding failed");
    }

    if (!Data.IsUploadReady())
    {
        throw std::runtime_error("Basis texture produced no upload payload");
    }

    const vk::Format ExpectedFormat = GetBasisTargetFormat(Target, Data.GetColorSpace());

    if (Data.GetFormat() != ExpectedFormat)
    {
        throw std::runtime_error("Basis transcode produced an unexpected format");
    }
}

BasisTranscodeTarget RenderResourceCache::SelectBasisTranscodeTarget(const TextureData& Data) const
{
    constexpr std::array Candidates = {
        BasisTranscodeTarget::BC7RGBA,
        BasisTranscodeTarget::ASTC4x4RGBA,
        BasisTranscodeTarget::ETC2RGBA,
        BasisTranscodeTarget::BC3RGBA,
        BasisTranscodeTarget::RGBA32
    };

    for (BasisTranscodeTarget Target : Candidates)
    {
        if (IsBasisTargetSupported(Data, Target))
        {
            return Target;
        }
    }

    throw std::runtime_error("No supported Basis transcode target");
}

bool RenderResourceCache::IsBasisTargetSupported(const TextureData& Data, BasisTranscodeTarget Target) const
{
    const vk::Format Format = GetBasisTargetFormat(Target, Data.GetColorSpace());

    if (Format == vk::Format::eUndefined)
    {
        return false;
    }

    vk::ImageCreateFlags CreateFlags;

    if (Data.GetFaceCount() == 6)
    {
        CreateFlags |= vk::ImageCreateFlagBits::eCubeCompatible;
    }

    const vk::ImageUsageFlags Usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;

    vk::ImageFormatProperties Properties;

    try
    {
        Properties = PhysicalDevice.getImageFormatProperties(
                Format,
                vk::ImageType::e2D,
                vk::ImageTiling::eOptimal,
                Usage,
                CreateFlags);
    }
    catch (const vk::SystemError&)
    {
        return false;
    }

    const uint64_t ArrayLayers = static_cast<uint64_t>(Data.GetLayerCount()) * static_cast<uint64_t>(Data.GetFaceCount());

    if (Data.GetWidth() > Properties.maxExtent.width ||
        Data.GetHeight() > Properties.maxExtent.height ||
        Data.GetDepth() > Properties.maxExtent.depth)
    {
        return false;
    }

    if (Data.GetMipLevels() > Properties.maxMipLevels)
    {
        return false;
    }

    if (ArrayLayers > Properties.maxArrayLayers)
    {
        return false;
    }

    if (!(Properties.sampleCounts & vk::SampleCountFlagBits::e1))
    {
        return false;
    }

    return true;
}
