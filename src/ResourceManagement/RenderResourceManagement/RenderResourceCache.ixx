module;

#include <string>
#include <memory>
#include <unordered_map>
#include <vulkan/vulkan_raii.hpp>

export module RenderResourceCache;

import Mesh;
import Shader;
import MeshData;
import ShaderData;
import VulkanUploader;

export class RenderResourceCache
{
public:
    RenderResourceCache(
        const vk::raii::Device& Device,
        const vk::raii::PhysicalDevice& PhysicalDevice,
        VulkanUploader* Uploader)
		: Device(Device), 
        PhysicalDevice(PhysicalDevice),
        UploaderPtr(Uploader)
    {}

    Mesh* GetOrUploadMesh(const std::string& ID, const MeshData& Data);
    Shader* GetOrCompileShader(const std::string& ID, const ShaderData& Data);

    void Evict(const std::string& ID);
    void EvictAll();

private:
    const vk::raii::Device& Device;
    const vk::raii::PhysicalDevice& PhysicalDevice;
    VulkanUploader* UploaderPtr;

    std::unordered_map<std::string, std::unique_ptr<Mesh>>   MeshCache;
    std::unordered_map<std::string, std::unique_ptr<Shader>> ShaderCache;
};