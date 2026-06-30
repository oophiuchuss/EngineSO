module;

#include <vulkan/vulkan_raii.hpp>

#include <vector>
#include <unordered_set>

export module Renderer;

import Rendergraph;
import CullingSystem;
import CameraComponent;
import Entity;
import CameraUniform;
import Scene;
import ResourceManager;
import RenderResourceCache;
import PipelineCache;
import EventListener;
import VulkanUploader;
import DescriptorHeap;
import GPUSceneBuffer;
import GBufferDescriptorSet;
import SingleTextureDescriptorSet;
import LightBuffer;

import MeshData;
import TextureData;

export class Renderer : public EventListener
{
public:
	Renderer(vk::raii::Instance& Instance, vk::raii::SurfaceKHR&& Surface, ResourceManager* InResourceManagerPtr);
	~Renderer();

	void RenderFrame(Scene* SceneToRender);

private:
	void SetActiveCamera(CameraComponent* Camera);
	void RecreateSwapchain();
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSwapchain();
	void CreateSyncObjects();
	void SetupRenderPasses();

	void PreloadSceneResources(Scene& Scene);
	void PreloadEntityResources(Entity& Entity);

	void CollectEntityResources(
		Entity& Entity,
		std::unordered_set<std::string>& SeenTextures,
		std::vector<std::string>& TextureIDs,
		std::vector<const TextureData*>& TextureDataPtrs,
		std::unordered_set<std::string>& SeenMeshes,
		std::vector<std::string>& MeshIDs,
		std::vector<const MeshData*>& MeshDataPtrs);

	int ScorePhysicalDevice(const vk::raii::PhysicalDevice& Dev, const vk::raii::SurfaceKHR& Surface);

	bool IsFormatUsageSupported(vk::Format Format, vk::ImageUsageFlags Usage);

	bool CanAcquireSwapchainImage() const;
	
	void OnEvent(const EventBase& Event) override;

	ResourceManager* ResourceManagerPtr;	// Non-owning resource manager

	vk::raii::Instance& Instance;			// Non-owning instance
	vk::raii::SurfaceKHR Surface;			// Owning surface, moved in

	vk::raii::PhysicalDevice PhysicalDevice = nullptr;
	vk::raii::Device Device = nullptr;

	// Queues
	vk::raii::Queue GraphicsQueue	= nullptr;
	vk::raii::Queue PresentQueue	= nullptr;
	vk::raii::Queue TransferQueue	= nullptr; // TODO: For future async 

	uint32_t GraphicsQueueFamilyIndex = 0;
	uint32_t TransferQueueFamilyIndex = 0;

	// Swapchain
	vk::raii::SwapchainKHR Swapchain = nullptr;
	std::vector<vk::Image> SwapchainImages;					// Should be non-raii as owned by swapchain
	std::vector<vk::raii::ImageView> SwapchainImageViews;	// Created ourselves therefore is raii
	vk::Format SwapchainImageFormat;
	vk::Extent2D SwapchainExtent;

	// Synchronization (double buffered, but swapchain may have 3 images)
	static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
	std::vector<vk::raii::Semaphore> ImageAvailableSemaphores;
	std::vector<vk::raii::Semaphore> RenderFinishedSemaphores;
	std::vector<vk::raii::Fence>     InFlightFences;
	int CurrentFrame = 0;

	vk::raii::CommandPool CommandPool = nullptr;
	std::vector<vk::raii::CommandBuffer>  CommandBuffers;

	std::unique_ptr<Rendergraph>				RendergraphInstance;
	std::unique_ptr<CullingSystem>				CullingSystemInstance;
	std::unique_ptr<CameraUniformBuffer>		CameraUBO;
	std::unique_ptr<VulkanUploader>				UploaderInstance;
	std::unique_ptr<DescriptorHeap>				DescriptorHeapInstance;
	std::unique_ptr<GPUSceneBuffer>				GPUSceneInstance;
	std::unique_ptr<GBufferDescriptorSet>		GBufferDescSet;
	std::unique_ptr<SingleTextureDescriptorSet> ToneMapInputDesc;
	std::unique_ptr<SingleTextureDescriptorSet> GammaInputDesc;
	std::unique_ptr<SingleTextureDescriptorSet> FinalOutputDesc;
	std::unique_ptr<LightBuffer>				LightBufferInstance;
	std::unique_ptr<RenderResourceCache>		RenderCacheInstance;
	std::unique_ptr<PipelineCache>				PipelineCacheInstance;
};