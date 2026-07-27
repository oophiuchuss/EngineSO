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

    class DescriptorAllocationGuard
    {
    public:
        DescriptorAllocationGuard(
            DescriptorHeap& InHeap,
            TextureDescriptorAllocation InAllocation) :
            Heap(&InHeap),
            Allocation(InAllocation)
        {
        }

        DescriptorAllocationGuard(const DescriptorAllocationGuard&) = delete;

        DescriptorAllocationGuard& operator=(const DescriptorAllocationGuard&) = delete;

        ~DescriptorAllocationGuard()
        {
            if (Heap && Allocation.IsValid())
            {
                Heap->FreeSlot(Allocation);
            }
        }

        void Release()
        {
            Heap = nullptr;
        }

    private:
        DescriptorHeap* Heap = nullptr;
        TextureDescriptorAllocation Allocation;
    };
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

int RenderResourceCache::GetOrUploadTexture2D(const std::string& ID, TextureData& Data)
{
    return GetOrUploadTexture(ID, Data, TextureDescriptorType::Texture2D);
}

int RenderResourceCache::GetOrUploadCubemap(const std::string& ID, TextureData& Data)
{
    return GetOrUploadTexture(ID, Data, TextureDescriptorType::Cubemap);
}

std::vector<int> RenderResourceCache::GetOrUploadTexture2DBatch(const std::vector<std::string>& IDs, const std::vector<TextureData*>& DataList)
{
    if (IDs.size() != DataList.size())
    {
        throw std::invalid_argument("GetOrUploadTexture2DBatch: IDs and DataList size mismatch");
    }

    std::vector<int> ResultSlots(IDs.size(), -1);

    std::vector<size_t> PendingIndices;
    std::vector<TextureData*> PendingData;

    PendingIndices.reserve(IDs.size());
    PendingData.reserve(DataList.size());

    for (size_t Index = 0; Index < IDs.size(); ++Index)
    {
        TextureData* Data = DataList[Index];

        if (!Data)
        {
            throw std::runtime_error("Texture batch contains null data");
        }

        ValidateTextureShape(*Data, TextureDescriptorType::Texture2D);

        auto Existing = TextureAllocationMap.find(IDs[Index]);

        if (Existing != TextureAllocationMap.end())
        {
            if (Existing->second.Type != TextureDescriptorType::Texture2D)
            {
                throw std::runtime_error("Cached texture is not a Texture2D: " + IDs[Index]);
            }

            ResultSlots[Index] = Existing->second.Slot;

            continue;
        }

        PendingIndices.push_back(Index);
        PendingData.push_back(Data);
    }

    if (PendingData.empty())
    {
        return ResultSlots;
    }

    for (TextureData* Data : PendingData)
    {
        PrepareTextureForUpload(*Data);
    }

    std::vector<const TextureData*> PreparedData(PendingData.begin(), PendingData.end());

    std::vector<std::unique_ptr<Texture>> NewTextures = Texture::CreateBatchFromTextureData(Device, *UploaderPtr, PreparedData);

    if (NewTextures.size() != PendingData.size())
    {
        throw std::runtime_error("GetOrUploadTexture2DBatch: batch upload returned mismatched count");
    }

    for (size_t PendingIndex = 0; PendingIndex < NewTextures.size(); PendingIndex++)
    {
        const size_t OriginalIndex = PendingIndices[PendingIndex];

        const std::string& ID = IDs[OriginalIndex];

        TextureDescriptorAllocation Allocation = DescriptorHeapPtr->AllocateSlot(TextureDescriptorType::Texture2D);

        if (!Allocation.IsValid())
        {
            ResultSlots[OriginalIndex] = -1;
            continue;
        }

        DescriptorAllocationGuard AllocationGuard(
            *DescriptorHeapPtr,
            Allocation);

        DescriptorHeapPtr->WriteSlot(
            Allocation,
            NewTextures[PendingIndex]->GetImageView(),
            DataList[OriginalIndex]->GetSamplerDesc());

        TextureCache[ID] = std::move(NewTextures[PendingIndex]);
        TextureAllocationMap[ID] = Allocation;
        ResultSlots[OriginalIndex] = Allocation.Slot;

        AllocationGuard.Release();
    }

    return ResultSlots;
}

void RenderResourceCache::Evict(const std::string& ID)
{
    MeshCache.erase(ID);
    ShaderCache.erase(ID);

    auto AllocationIt = TextureAllocationMap.find(ID);

    if (AllocationIt != TextureAllocationMap.end())
    {
        DescriptorHeapPtr->FreeSlot(AllocationIt->second);
        TextureAllocationMap.erase(AllocationIt);
    }

    TextureCache.erase(ID);
}

void RenderResourceCache::EvictAll()
{
    MeshCache.clear();
    ShaderCache.clear();

    for (const auto& [ID, Allocation] : TextureAllocationMap)
    {
        DescriptorHeapPtr->FreeSlot(Allocation);
    }

    TextureAllocationMap.clear();
    TextureCache.clear();
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

int RenderResourceCache::GetOrUploadTexture(const std::string& ID, TextureData& Data, TextureDescriptorType ExpectedType)
{
    ValidateTextureShape(Data, ExpectedType);

    auto Existing = TextureAllocationMap.find(ID);

    if (Existing != TextureAllocationMap.end())
    {
        if (Existing->second.Type != ExpectedType)
        {
            throw std::runtime_error("Cached texture descriptor type does not match the requested descriptor type: " + ID);
        }

        return Existing->second.Slot;
    }

    PrepareTextureForUpload(Data);

    std::unique_ptr<Texture> NewTexture = Texture::CreateFromTextureData(Device, *UploaderPtr, Data);

    if (!NewTexture)
    {
        return -1;
    }

    TextureDescriptorAllocation Allocation = DescriptorHeapPtr->AllocateSlot(ExpectedType);

    if (!Allocation.IsValid())
    {
        return -1;
    }

    DescriptorAllocationGuard AllocationGuard(*DescriptorHeapPtr, Allocation);

    DescriptorHeapPtr->WriteSlot(Allocation, NewTexture->GetImageView(), Data.GetSamplerDesc());

    TextureCache[ID] = std::move(NewTexture);
    TextureAllocationMap[ID] = Allocation;

    AllocationGuard.Release();

    return Allocation.Slot;
}

void RenderResourceCache::ValidateTextureShape(const TextureData& Data, TextureDescriptorType ExpectedType) const
{
    if (Data.GetDepth() != 1)
    {
        throw std::runtime_error("Only 2D texture images are supported by the current descriptor heap");
    }

    switch (ExpectedType)
    {
    case TextureDescriptorType::Texture2D:
        if (Data.GetFaceCount() != 1 || Data.GetLayerCount() != 1)
        {
            throw std::runtime_error("Sampler2D requires one face and one layer");
        }
        break;

    case TextureDescriptorType::Cubemap:
        if (Data.GetFaceCount() != 6 || Data.GetLayerCount() != 1)
        {
            throw std::runtime_error("SamplerCube requires six faces and one cube layer");
        }

        if (Data.GetWidth() != Data.GetHeight())
        {
            throw std::runtime_error("Cubemap faces must be square");
        }
        break;

    default:
        throw std::runtime_error("Unsupported texture descriptor type");
    }
}
