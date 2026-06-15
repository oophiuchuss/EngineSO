module;

#include <string>
#include <memory>
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