module;

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <vulkan/vulkan_raii.hpp>

export module RenderResourceCache;

import VulkanUploader;
import DescriptorHeap;
import Mesh;
import Shader;
import Texture;
import MeshData;
import ShaderData;
import TextureData;

export class RenderResourceCache
{
public:
    RenderResourceCache(
        const vk::raii::Device& Device,
        const vk::raii::PhysicalDevice& PhysicalDevice,
        VulkanUploader* Uploader,
        DescriptorHeap* InDescriptorHeap)
		: Device(Device), 
        PhysicalDevice(PhysicalDevice),
        UploaderPtr(Uploader),
        DescriptorHeapPtr(InDescriptorHeap)
    {}

    Mesh* GetOrUploadMesh(const std::string& ID, const MeshData& Data);
    Shader* GetOrCompileShader(const std::string& ID, const ShaderData& Data);

    void GetOrUploadMeshBatch(const std::vector<std::string>& IDs, const std::vector<const MeshData*>& DataList);

    int GetOrUploadTexture2D(const std::string& ID, TextureData& Data);
    int GetOrUploadCubemap(const std::string& ID, TextureData& Data);

    // IDs and DataList must be the same size and correspond by index
    // Returns slot indices in the same order as the input
    std::vector<int> GetOrUploadTexture2DBatch(const std::vector<std::string>& IDs, const std::vector<TextureData*>& DataList);

    // TODO: maybe resolve in better way
    inline bool IsMeshCached(const std::string& ID) const
    {
        return MeshCache.find(ID) != MeshCache.end();
    }

    // TODO: maybe resolve in better way
    inline bool IsTextureCached(const std::string& ID) const
    {
        return TextureAllocationMap.find(ID) != TextureAllocationMap.end();
    }

    void Evict(const std::string& ID);
    void EvictAll();

private:
	// Helper function to prepare a texture for upload, including transcoding if necessary
    void PrepareTextureForUpload(TextureData& Data);
    BasisTranscodeTarget SelectBasisTranscodeTarget(const TextureData& Data) const;
    bool IsBasisTargetSupported(const TextureData& Data, BasisTranscodeTarget Target) const;

    int GetOrUploadTexture(const std::string& ID, TextureData& Data, TextureDescriptorType ExpectedType);
    void ValidateTextureShape(const TextureData& Data, TextureDescriptorType ExpectedType) const;

    const vk::raii::Device& Device;
    const vk::raii::PhysicalDevice& PhysicalDevice;
    VulkanUploader* UploaderPtr;
    DescriptorHeap* DescriptorHeapPtr;

    std::unordered_map<std::string, std::unique_ptr<Mesh>> MeshCache;
    std::unordered_map<std::string, std::unique_ptr<Shader>> ShaderCache;
    std::unordered_map<std::string, std::unique_ptr<Texture>> TextureCache;
    std::unordered_map<std::string, TextureDescriptorAllocation> TextureAllocationMap;
};