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
    int GetOrUploadTexture(const std::string& ID, const TextureData& Data);

    void GetOrUploadMeshBatch(const std::vector<std::string>& IDs, const std::vector<const MeshData*>& DataList);

    // IDs and DataList must be the same size and correspond by index
    // Returns slot indices in the same order as the input
    std::vector<int> GetOrUploadTextureBatch(const std::vector<std::string>& IDs, const std::vector<const TextureData*>& DataList);

    // TODO: maybe resolve in better way
    inline bool IsMeshCached(const std::string& ID) const
    {
        return MeshCache.find(ID) != MeshCache.end();
    }

    // TODO: maybe resolve in better way
    inline bool IsTextureCached(const std::string& ID) const
    {
        return TextureSlotMap.find(ID) != TextureSlotMap.end();
    }

    void Evict(const std::string& ID);
    void EvictAll();

private:
    const vk::raii::Device& Device;
    const vk::raii::PhysicalDevice& PhysicalDevice;
    VulkanUploader* UploaderPtr;
    DescriptorHeap* DescriptorHeapPtr;

    std::unordered_map<std::string, std::unique_ptr<Mesh>> MeshCache;
    std::unordered_map<std::string, std::unique_ptr<Shader>> ShaderCache;
    std::unordered_map<std::string, std::unique_ptr<Texture>> TextureCache;
    std::unordered_map<std::string, int> TextureSlotMap;    // Map id to slot index
};