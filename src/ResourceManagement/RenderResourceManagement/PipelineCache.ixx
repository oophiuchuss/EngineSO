module;

#include <memory>
#include <unordered_map>
#include <string>
#include <vulkan/vulkan_raii.hpp>

export module PipelineCache;

import Shader;

export struct PipelineKey
{
    Shader* ShaderPtr = nullptr;
    std::vector<vk::Format> ColorFormats;               // supports multiple color attachments
    vk::Format DepthFormat = vk::Format::eUndefined;
    std::vector<vk::DescriptorSetLayout> DescriptorSetLayouts;

    // Size = 0 means no push constants
    vk::PushConstantRange PushConstantRange;

    // False for fullscreen passes with no vertex buffer (e.g. LightingPass)
    bool bUseVertexInput = true;

    bool bEnableBlending = false;
    bool bDepthWriteEnable = true;

    bool operator==(const PipelineKey& Other) const
    {
        return ShaderPtr == Other.ShaderPtr
            && ColorFormats == Other.ColorFormats
            && DepthFormat == Other.DepthFormat
            && DescriptorSetLayouts == Other.DescriptorSetLayouts
            && PushConstantRange.stageFlags == Other.PushConstantRange.stageFlags
            && PushConstantRange.size == Other.PushConstantRange.size
            && PushConstantRange.offset == Other.PushConstantRange.offset
            && bUseVertexInput == Other.bUseVertexInput
            && bEnableBlending == Other.bEnableBlending
            && bDepthWriteEnable == Other.bDepthWriteEnable;
    }
};

export struct PipelineKeyHash
{
    size_t operator()(const PipelineKey& Key) const
    {
        // Start with shader pointer
        size_t Seed = std::hash<void*>{}(Key.ShaderPtr);

        // Fold in each color format
        for (auto Fmt : Key.ColorFormats)
        {
            Seed ^= std::hash<uint32_t>{}(static_cast<uint32_t>(Fmt))
                + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
        }

        // Fold in depth format
        Seed ^= std::hash<uint32_t>{}(static_cast<uint32_t>(Key.DepthFormat))
            + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);

        for (auto Layout : Key.DescriptorSetLayouts)
        {
            Seed ^= std::hash<void*>{}(static_cast<void*>(Layout))  // raw handle pointer
                + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
        }

        Seed ^= std::hash<uint32_t>{}(static_cast<uint32_t>(Key.PushConstantRange.stageFlags))
            + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
        
        Seed ^= std::hash<uint32_t>{}(Key.PushConstantRange.size)
            + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
        
        Seed ^= std::hash<uint32_t>{}(Key.PushConstantRange.offset)
            + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);

        Seed ^= std::hash<bool>{}(Key.bUseVertexInput)
            + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);

        Seed ^= std::hash<bool>{}(Key.bEnableBlending)
            + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);

        Seed ^= std::hash<bool>{}(Key.bDepthWriteEnable)
            + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);

        return Seed;
    }
};

struct PipelineCacheEntry
{
    vk::raii::Pipeline Pipeline;
    vk::raii::PipelineLayout PipelineLayout;
};

export struct PipelineHandles
{
    vk::Pipeline Pipeline;
    vk::PipelineLayout Layout;
};

export class PipelineCache
{
public:
    PipelineCache(const vk::raii::Device& Device,
        const vk::raii::PhysicalDevice& PhysicalDevice,
        const std::string& CacheFilePath = "");

	~PipelineCache();

    // Returns both pipeline and layout in one lookup — avoids double search
    PipelineHandles GetOrCreate(const PipelineKey& Key);
   
	// TODO: use somewhere eviction functions when shader is hot-reloaded or destroyed. For now, they are just public API that can be called manually when needed.
    // 
    // Evict a specific key
    void Evict(const PipelineKey& Key);

    // Evict all pipelines compiled from a specific shader
    // (useful when a shader is hot-reloaded or destroyed)
    void EvictAllForShader(Shader* ShaderPtr);

    // Evict everything
    void EvictAll();

    // Serialize compiled pipeline cache to disk for faster startup next run
    void SaveToDisk() const;

private:
    
	PipelineCacheEntry* CreateCacheEntry(const PipelineKey& Key);

    const vk::raii::Device& Device;
    const vk::raii::PhysicalDevice& PhysicalDevice;
    std::string CacheFilePath;

    vk::raii::PipelineCache CurrentVkPipelineCache = nullptr;

	std::unordered_map<PipelineKey, std::unique_ptr<PipelineCacheEntry>, PipelineKeyHash> Cache;
};