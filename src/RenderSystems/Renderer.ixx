module;

#include <vulkan/vulkan_raii.hpp>

export module Renderer;

import Rendergraph;
import CullingSystem;
import Entity;

export class Renderer
{
public:
	Renderer(vk::raii::Device& Dev, vk::Queue Queue, vk::raii::PhysicalDevice& PhysicalDevice, Camera* Camera);

	inline void SetCamera(Camera* NewCamera)
	{
		CullingSystem.SetCamera(NewCamera);
	}

	void Render(const std::vector<Entity*>& Entities);

private:
	void SetupRenderPasses();

	vk::raii::Device& Device;
	vk::Queue GraphicsQueue;
	vk::raii::CommandPool CommandPool = nullptr;

	Rendergraph Rendergraph;
	CullingSystem CullingSystem;

	// Current frame resources
	vk::raii::CommandBuffer CmdBuffer = nullptr;
	vk::raii::Fence Fence = nullptr;
	vk::raii::Semaphore ImageAvailableSemaphore = nullptr;
	vk::raii::Semaphore RenderFinishedSemaphore = nullptr;

	int WindowWidth = 1920;
	int WindowHeight = 1080;
};