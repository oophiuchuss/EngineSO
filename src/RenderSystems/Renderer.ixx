module;

#include <vulkan/vulkan_raii.hpp>


#include <vector>

export module Renderer;

import Rendergraph;
import CullingSystem;
import CameraComponent;
import Entity;
import CameraUniform;
import Scene;

import Shader;	// TODO: For now, but should be per material
import Mesh;	// TODO: For now, but should be per material
import RenderPassBase;



export class Renderer
{
public:
	Renderer(vk::raii::Instance& Instance, vk::raii::SurfaceKHR&& Surface);
	~Renderer();

	void RenderFrame(Scene* SceneToRender);
	void RecreateSwapchain();

	void SetActiveCamera(CameraComponent* Camera); // TODO: make better place where to get active camera

	std::shared_ptr<Mesh> GetTestTriangleMesh() { return TesttriangleMesh; } // TODO: For now, but should be removed

private:
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSwapchain();
	void CreateSyncObjects();
	void SetupRenderPasses();

	int ScorePhysicalDevice(const vk::raii::PhysicalDevice& Dev, const vk::raii::SurfaceKHR& Surface);

	bool IsFormatUsageSupported(vk::Format Format, vk::ImageUsageFlags Usage);

	bool CanAcquireSwapchainImage() const;
	
	vk::raii::Instance& Instance;	// Non-owning instance
	vk::raii::SurfaceKHR Surface;	// Owning surface, moved in

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

	std::unique_ptr<Rendergraph>   RendergraphPtr;
	std::unique_ptr<CullingSystem> CullingSystemPtr;
	std::unique_ptr<CameraUniformBuffer> CameraUBO;

	// TODO: For now, but should be per material
	std::unique_ptr<Shader> DefaultShader;			
	vk::raii::PipelineLayout DefaultPipelineLayout = nullptr;
	vk::raii::Pipeline DefaultPipeline = nullptr;	

	std::shared_ptr<Mesh> TesttriangleMesh; // TODO: For now, but should be removed

};