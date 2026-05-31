module; 

#include <vulkan/vulkan_raii.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <array>

module PipelineCache;

import Geometry;


PipelineCache::PipelineCache(
	const vk::raii::Device& Device, 
	vk::DescriptorSetLayout CameraUBOLayout, 
	const std::string& CacheFilePath)
	: Device(Device),
	CameraUBOLayout(CameraUBOLayout),
	CacheFilePath(CacheFilePath)
{
	// Try to load previouslt serialized cache from disk
	std::vector<char> CacheData;
	if (!CacheFilePath.empty())
	{
		std::ifstream CacheFile(CacheFilePath, std::ios::binary | std::ios::ate);
		if (CacheFile)
		{
			auto Size = CacheFile.tellg();
			CacheData.resize(static_cast<size_t>(Size));
			CacheFile.seekg(0);
			CacheFile.read(CacheData.data(), Size);
		}
	}

	vk::PipelineCacheCreateInfo CacheInfo(
		{}, 
		CacheData.size(),
		CacheData.empty() ? nullptr : CacheData.data());

	VkPipelineCache = Device.createPipelineCache(CacheInfo);
}

PipelineCache::~PipelineCache()
{
	SaveToDisk();
}

PipelineHandles PipelineCache::GetOrCreate(const PipelineKey& Key)
{
	auto It = Cache.find(Key);
	if (It != Cache.end())
	{
		return { *(It->second->Pipeline), *(It->second->PipelineLayout) };
	}

	PipelineCacheEntry* Entry = CreateCacheEntry(Key);
	return { *(Entry->Pipeline), *(Entry->PipelineLayout) };
}

void PipelineCache::Evict(const PipelineKey& Key)
{
	Cache.erase(Key);
}

void PipelineCache::EvictAllForShader(Shader* ShaderPtr)
{
	for (auto It = Cache.begin(); It != Cache.end(); )
	{
		if (It->first.ShaderPtr == ShaderPtr)
		{
			It = Cache.erase(It);
		}
		else
		{
			++It;
		}
	}
}

void PipelineCache::EvictAll()
{
	Cache.clear();
}

void PipelineCache::SaveToDisk() const
{
	if (CacheFilePath.empty() || VkPipelineCache == nullptr)
		return;

	auto Data = VkPipelineCache.getData();
	if (Data.empty()) return;

	std::ofstream File(CacheFilePath, std::ios::binary | std::ios::trunc);
	if (!File) return;

	File.write(reinterpret_cast<const char*>(Data.data()), Data.size());
}

PipelineCacheEntry* PipelineCache::CreateCacheEntry(const PipelineKey& Key)
{
	vk::PipelineLayoutCreateInfo PipelineLayoutInfo({}, { CameraUBOLayout });
	vk::raii::PipelineLayout PipelineLayout = vk::raii::PipelineLayout(Device, PipelineLayoutInfo);

	auto Stages = Key.ShaderPtr->GetShaderStageInfos();

	vk::VertexInputBindingDescription BindingDesc(0, sizeof(Vertex), vk::VertexInputRate::eVertex);

	std::array<vk::VertexInputAttributeDescription, 3> AttribDescs = { {
		{ 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Position) },
		{ 1, 0, vk::Format::eR32G32Sfloat,     offsetof(Vertex, UV) },
		{ 2, 0, vk::Format::eR32G32B32Sfloat,  offsetof(Vertex, Normal) },
	} };

	vk::PipelineVertexInputStateCreateInfo VertexInputInfo({}, BindingDesc, AttribDescs);
	vk::PipelineInputAssemblyStateCreateInfo InputAssemblyInfo({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

	// Viewport and scissor will be dynamic states, so we don't specify them here
	vk::PipelineViewportStateCreateInfo ViewportStateInfo({}, 1, nullptr, 1, nullptr);

	vk::PipelineRasterizationStateCreateInfo RasterizerInfo(
		{}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise,
		VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);

	vk::PipelineMultisampleStateCreateInfo MultisampleInfo(
		{}, vk::SampleCountFlagBits::e1, VK_FALSE);

	vk::PipelineDepthStencilStateCreateInfo DepthStencilInfo(
		{}, VK_TRUE, VK_TRUE, vk::CompareOp::eLess,
		VK_FALSE, VK_FALSE);

	vk::PipelineColorBlendAttachmentState ColorBlendAttachment;
	ColorBlendAttachment.setColorWriteMask(
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
		vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

	ColorBlendAttachment.setBlendEnable(VK_FALSE);

	vk::PipelineColorBlendStateCreateInfo ColorBlendInfo(
		{}, VK_FALSE, vk::LogicOp::eCopy, ColorBlendAttachment);

	vk::DynamicState DynamicStates[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo DynamicStateInfo({}, DynamicStates);

	vk::PipelineRenderingCreateInfo DynamicRenderingInfo(
		{},								// viewMask (0)
		Key.ColorFormats,				// colour attachment formats
		Key.DepthFormat					// depth format
	);

	vk::GraphicsPipelineCreateInfo PipelineInfo(
		{},
		Stages,
		&VertexInputInfo,
		&InputAssemblyInfo,
		nullptr,				// No tessellation
		&ViewportStateInfo,
		&RasterizerInfo,
		&MultisampleInfo,
		&DepthStencilInfo,
		&ColorBlendInfo,
		&DynamicStateInfo,
		PipelineLayout,
		nullptr,				// No render pass since we're using dynamic rendering
		0,						// Subpass
		nullptr,				// No base pipeline
		0);						// No base pipeline index

	PipelineInfo.setPNext(&DynamicRenderingInfo); // Chain dynamic rendering info to pipeline create info

	vk::raii::Pipeline Pipeline = Device.createGraphicsPipeline(VkPipelineCache, PipelineInfo);

	// Store in cache and return
	auto Entry = std::make_unique<PipelineCacheEntry>(PipelineCacheEntry(std::move(Pipeline), std::move(PipelineLayout)));

	PipelineCacheEntry* EntryRaw = Entry.get();
	Cache[Key] = std::move(Entry);

	return EntryRaw;
}
