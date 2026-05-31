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
    vk::Format              DepthFormat = vk::Format::eUndefined;

    bool operator==(const PipelineKey& Other) const
    {
        return ShaderPtr == Other.ShaderPtr
            && ColorFormats == Other.ColorFormats
            && DepthFormat == Other.DepthFormat;
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

        return Seed;
    }
};

struct PipelineCacheEntry
{
    vk::raii::Pipeline       Pipeline;
    vk::raii::PipelineLayout PipelineLayout;
};

export struct PipelineHandles
{
    vk::Pipeline       Pipeline;
    vk::PipelineLayout Layout;
};

export class PipelineCache
{
public:
    PipelineCache(const vk::raii::Device& Device,
        vk::DescriptorSetLayout CameraUBOLayout,
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
    vk::DescriptorSetLayout CameraUBOLayout;
    std::string             CacheFilePath;

    vk::raii::PipelineCache VkPipelineCache = nullptr;

	std::unordered_map<PipelineKey, std::unique_ptr<PipelineCacheEntry>, PipelineKeyHash> Cache;
};