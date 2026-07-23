module;

#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <memory>
#include <iostream>

module Renderer;

import Paths;
import GPUProfiler;
import Rendergraph;
import CullingSystem;
import Entity;
import CameraUniform;
import Scene;
import ResourceManager;
import ResourceHandle;
import VulkanUploader;
import DescriptorHeap;
import GPUSceneBuffer;
import GPUSceneData;
import GBufferDescriptorSet;
import SingleTextureDescriptorSet;
import LightBuffer;
import GPULightData;
import RenderResourceCache;
import FrameData;

import GeometryRenderPass;
import LightingPass;
import ForwardTranslucencyPass;
import PostProcessPass;
import ImGuiRenderPass;

import EventDispatcher;
import WindowResizeEvent;
import SceneBulkChangedEvent;
import SceneEntityChangedEvent;
import ResourceReprocessedEvent;
import PostProcessSettingsChangedEvent;

import MeshComponent;
import TransformComponent;
import LightComponentBase;
import DirectionalLightComponent;
import SpotLightComponent;
import PointLightComponent;
import MeshData;
import ShaderData;
import TextureData;
import Shader;
import Mesh;
import Texture;
import TextureSlots;
import Material;
import MaterialProperties;
import Geometry;

Renderer::Renderer(
	vk::raii::Instance& Instance, 
	vk::raii::SurfaceKHR&& Surface, 
	ResourceManager* InResourceManagerPtr) :
	Instance(Instance), 
	Surface(std::move(Surface)), 
	ResourceManagerPtr(InResourceManagerPtr)
{
	// Pick physical device
	PickPhysicalDevice();
	// Create logical device and queues
	CreateLogicalDevice();
	// Create swapchain and related resources
	CreateSwapchain();
	// Create synchronization primitives
	CreateSyncObjects();

	ProfilerInstance = std::make_unique<GPUProfiler>(
		Device, 
		PhysicalDevice, 
		MAX_FRAMES_IN_FLIGHT);

	RendergraphInstance = std::make_unique<Rendergraph>(
		Device, 
		PhysicalDevice);

	CullingSystemInstance = std::make_unique<CullingSystem>();

	CameraUBO = std::make_unique<CameraUniformBuffer>(
		Device, 
		PhysicalDevice,
		MAX_FRAMES_IN_FLIGHT);

	UploaderInstance = std::make_unique<VulkanUploader>(
		Device,
		PhysicalDevice,
		TransferQueueFamilyIndex,
		GraphicsQueueFamilyIndex,
		TransferQueue,
		GraphicsQueue);
	
	DescriptorHeapInstance = std::make_unique<DescriptorHeap>(
		PhysicalDevice,
		Device,
		1024,						// TODO: Max textures hard-coded for now
		*UploaderInstance);	

	GPUSceneInstance = std::make_unique<GPUSceneBuffer>(
		Device,
		PhysicalDevice,
		MAX_FRAMES_IN_FLIGHT,
		4096,   // TODO: MaxObjects — hardcoded for now, matches DescriptorHeap's pattern
		512);   // TODO: MaxMaterials — hardcoded for now

	LightBufferInstance = std::make_unique<LightBuffer>(
		Device,
		PhysicalDevice, 
		MAX_FRAMES_IN_FLIGHT,
		128);	// TODO: MaxLights should be handled better

	RenderCacheInstance = std::make_unique<RenderResourceCache>(
		Device, 
		PhysicalDevice,
		UploaderInstance.get(),
		DescriptorHeapInstance.get());

	PipelineCacheInstance = std::make_unique<PipelineCache>(
		Device, 
		PhysicalDevice, 
		Paths::GetCacheRoot() + "pipeline.bin");

	Shader* NormalMipShader = LoadShader("normal_mip");

	UploaderInstance->InitializeNormalMipGeneration(*NormalMipShader, *PipelineCacheInstance);

	// Create command pool and buffers
	vk::CommandPoolCreateInfo PoolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, 
									   GraphicsQueueFamilyIndex);

	CommandPool = vk::raii::CommandPool(Device, PoolInfo);

	vk::CommandBufferAllocateInfo CmdAllocInfo(*CommandPool, vk::CommandBufferLevel::ePrimary, MAX_FRAMES_IN_FLIGHT);

	CommandBuffers = std::move(Device.allocateCommandBuffers(CmdAllocInfo));

	// Set up render passes and framebuffers in the rendergraph
	SetupRenderPasses();
	RendergraphInstance->Compile();
	GBufferDescSet->Initialize(*RendergraphInstance);
	PostProcessDesc->Initialize(*RendergraphInstance);

	InitializeImGuiVulkanBackend();
}

Renderer::~Renderer()
{
	if (Device != nullptr)
	{
		// vk::raii::Device is truthy if valid
		Device.waitIdle();
		ShutdownImGuiVulkanBackend();
	}
}

void Renderer::RenderFrame(Scene* SceneToRender, ImDrawData* InImGuiDrawData)
{
	// Wait for previous frame to finish
	vk::Result FenceResult = Device.waitForFences({ *InFlightFences[CurrentFrame] }, VK_TRUE, UINT64_MAX);
	if (FenceResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to wait for in-flight fence");
	}

	// Check if swapchain is still compatible with the surface before acquiring an image, and recreate if not (e.g. window resized)
	if (!CanAcquireSwapchainImage())
	{
		RecreateSwapchain();
		return;
	}

	// Acquire next image from swapchain
	vk::Result AcquireNextImageResult = vk::Result::eErrorUnknown;
	uint32_t ImageIndex;

	try {
		// Check for the feature test macro for the new API
#ifdef VULKAN_HPP_RAII_VERSION_1
		auto AcquireResult = Swapchain.acquireNextImage(UINT64_MAX, *ImageAvailableSemaphores[CurrentFrame]);
		AcquireNextImageResult = AcquireResult.result;
		ImageIndex = AcquireResult.value;
#else
		// Fallback for older versions returning std::pair
		std::tie(AcquireNextImageResult, ImageIndex) = Swapchain.acquireNextImage(UINT64_MAX, *ImageAvailableSemaphores[CurrentFrame]);
#endif
	}
	catch (const vk::OutOfDateKHRError&) {
		AcquireNextImageResult = vk::Result::eErrorOutOfDateKHR;
	}
	catch (const vk::SystemError& e) {
		throw;   // re‑throw other unexpected errors
	}
	if (AcquireNextImageResult == vk::Result::eErrorOutOfDateKHR) {
		RecreateSwapchain();
		return;	
	}
	else if (AcquireNextImageResult != vk::Result::eSuccess && AcquireNextImageResult != vk::Result::eSuboptimalKHR) {
		throw std::runtime_error("Failed to acquire swapchain image");
	}

	// Provide the current swapchain image view to the graph
	RendergraphInstance->SetFrameExternalImage(
		"Swapchain",
		SwapchainImages[ImageIndex],           // vk::Image (non‑owning)
		*SwapchainImageViews[ImageIndex]);     // vk::ImageView

	// Reset the fence for the current frame only after successfully acquiring an image, to avoid waiting on a fence that won't be signaled if acquisition fails
	Device.resetFences({ *InFlightFences[CurrentFrame] }); 

	// Reset command buffer before usage
	auto& Cmd = CommandBuffers[CurrentFrame];
	Cmd.reset(); // Reset command buffer for recording
	
	vk::CommandBufferBeginInfo BeginInfo({});
	Cmd.begin(BeginInfo); // TODO: should this command buffer begin with no additional info?
	
	ProfilerInstance->BeginFrame(Cmd, CurrentFrame);
	
	{
		auto FrameScope = ProfilerInstance->BeginScope(Cmd, CurrentFrame, "TotalFrame");

		CameraComponent* CamComp = SceneToRender->GetActiveCameraComponent();
		TransformComponent* CamTrans = nullptr;

		if (CamComp)
		{
			SetActiveCamera(CamComp);

			CamTrans = CamComp->GetOwner()->GetComponent<TransformComponent>();

			CameraUniformData Data;
			Data.ViewProj = CamComp->GetProjectionMatrix() * CamComp->GetViewMatrix();
			Data.InverseViewProj = glm::inverse(Data.ViewProj);
			Data.CameraPos = glm::vec4(CamTrans->GetWorldPosition(), 1.0f);

			CameraUBO->Update(static_cast<uint32_t>(CurrentFrame),Data);
		}

		// Cull scene and update per-frame data
		CullingSystemInstance->CullScene(SceneToRender->GetRenderableEntities());

		// Build FrameData
		const auto& VisibleEntities = CullingSystemInstance->GetAllVisibleEntities();

		FrameData CurrentFrameData;
		CurrentFrameData.FrameIndex = static_cast<uint32_t>(CurrentFrame);
		CurrentFrameData.Camera = CameraUBO->GetLastData();
		CurrentFrameData.ImGuiDrawData = InImGuiDrawData;

		std::vector<ObjectData> FrameObjects;
		std::vector<MaterialData> FrameMaterials;
		std::unordered_map<const Material*, uint32_t> FrameMaterialIndices;
		FrameObjects.reserve(VisibleEntities.size());
		FrameMaterials.reserve(VisibleEntities.size());
		FrameMaterialIndices.reserve(VisibleEntities.size());

		auto GetOrCreateMaterialIndex = [&](const Material* Mat) -> uint32_t
			{
				auto Existing = FrameMaterialIndices.find(Mat);
				if (Existing != FrameMaterialIndices.end())
				{
					return Existing->second;
				}

				MaterialProperties Props = Mat ? Mat->GetMaterialProperties() : MaterialProperties{};

				if (Mat)
				{
					auto ResolveTexture = [&](const ResourceHandle<TextureData>& Handle, int DefaultIndex) -> int
						{
							if (const TextureData* TextureDataPtr = Handle.Get())
							{
								return RenderCacheInstance->GetOrUploadTexture(TextureDataPtr->GetResourceID(), *TextureDataPtr);
							}

							return DefaultIndex;
						};

					Props.AlbedoIndex = ResolveTexture(Mat->GetAlbedoTexture(), TextureSlots::DefaultWhite);
					Props.NormalIndex = ResolveTexture(Mat->GetNormalTexture(), TextureSlots::DefaultNormal);
					Props.MetallicRoughnessIndex = ResolveTexture(Mat->GetMetallicRoughnessTexture(), TextureSlots::DefaultWhite);
					Props.OcclusionIndex = ResolveTexture(Mat->GetOcclusionTexture(), TextureSlots::DefaultWhite);
					Props.EmissiveIndex = ResolveTexture(Mat->GetEmissiveTexture(), TextureSlots::DefaultBlack);
				}

				MaterialData Data;
				Data.AlbedoColor = Props.AlbedoColor;
				Data.Metallic = Props.Metallic;
				Data.Roughness = Props.Roughness;
				Data.EmissiveStrength = Props.EmissiveStrength;
				Data.NormalScale = Props.NormalScale;
				Data.AlbedoIndex = static_cast<uint32_t>(Props.AlbedoIndex);
				Data.NormalIndex = static_cast<uint32_t>(Props.NormalIndex);
				Data.MetallicRoughnessIndex = static_cast<uint32_t>(Props.MetallicRoughnessIndex);
				Data.OcclusionIndex = static_cast<uint32_t>(Props.OcclusionIndex);
				Data.EmissiveIndex = static_cast<uint32_t>(Props.EmissiveIndex);
				Data.AlphaMode = static_cast<uint32_t>(Props.AlphaMode);
				Data.AlphaCutoff = Props.AlphaCutoff;
				Data.NormalLodBias = Props.NormalLodBias;

				uint32_t NewIndex = static_cast<uint32_t>(FrameMaterials.size());

				FrameMaterials.push_back(Data);
				FrameMaterialIndices.emplace(Mat, NewIndex);

				return NewIndex;
			};


		for (Entity* E : VisibleEntities)
		{
			MeshComponent* MC = E->GetComponent<MeshComponent>();
			TransformComponent* TC = E->GetComponent<TransformComponent>();
			if (!MC || !TC) continue;

			// Resolve CPU data
			const MeshData* MD = MC->GetMeshData();
			if (!MD) continue;

			// Get GPU objects from cache (upload if necessary)
			Mesh* GPUMesh = RenderCacheInstance->GetOrUploadMesh(MD->GetResourceID(), *MD);
			if (!GPUMesh) continue;

			const Material* CurrentMaterial = MC->GetMaterial();

			uint32_t MaterialIndex = GetOrCreateMaterialIndex(CurrentMaterial);

			// Build ObjectData for this renderable 
			glm::mat4 WorldTransform = TC->GetWorldTransformMatrix();

			// Normal matrix computed once on CPU — inverse transpose of upper-left 3x3
			// Stored as mat4 to avoid GLM/GPU std430 mat3 alignment mismatch
			glm::mat3 NormalMat3 = glm::transpose(glm::inverse(glm::mat3(WorldTransform)));

			ObjectData Obj;
			Obj.ModelMatrix = WorldTransform;
			Obj.NormalMatrix = glm::mat4(NormalMat3); // upper-left 3x3 used by shader, rest is identity padding
			Obj.MaterialIndex = MaterialIndex;

			uint32_t ObjectIndex = static_cast<uint32_t>(FrameObjects.size());
			FrameObjects.push_back(Obj);

			// World-space bounds for culling
			BoundingBox WorldBounds = MD->GetBoundingBox();
			WorldBounds.Transform(WorldTransform);

			RenderableMesh NewRenderable = RenderableMesh(GPUMesh, WorldBounds, ObjectIndex);

			AlphaMode CurrentAlphaMode = CurrentMaterial? CurrentMaterial->GetMaterialProperties().AlphaMode : AlphaMode::Opaque;

			if (CurrentAlphaMode == AlphaMode::Blend)
			{
				CurrentFrameData.TranslucentRenderables.push_back(NewRenderable);
			}
			else
			{
				CurrentFrameData.Renderables.push_back(NewRenderable);
			}
		}

		glm::vec3 CamPos = CamTrans ? CamTrans->GetWorldPosition() : glm::vec3(0.0f);
		std::sort(CurrentFrameData.TranslucentRenderables.begin(), CurrentFrameData.TranslucentRenderables.end(),
			[&CamPos](const RenderableMesh& A, const RenderableMesh& B)
			{
				glm::vec3 CenterA = (A.WorldBounds.Min + A.WorldBounds.Max) * 0.5f;
				glm::vec3 CenterB = (B.WorldBounds.Min + B.WorldBounds.Max) * 0.5f;
				glm::vec3 DeltaA = CenterA - CamPos;
				glm::vec3 DeltaB = CenterB - CamPos;
				float DistA = glm::dot(DeltaA, DeltaA);
				float DistB = glm::dot(DeltaB, DeltaB);
				return DistA > DistB; // farthest first
			});


		GPUSceneInstance->Update(CurrentFrameData.FrameIndex, FrameObjects, FrameMaterials);

		// TODO: make better handling of this
		GeometryRenderPass* GPass = dynamic_cast<GeometryRenderPass*>(RendergraphInstance->GetRenderPass("GeometryPass"));
		if (GPass)
		{
			GPass->SetRenderArea(SwapchainExtent);
		}
		
		// TODO: make better handling of this
		ForwardTranslucencyPass* TransPass = dynamic_cast<ForwardTranslucencyPass*>(RendergraphInstance->GetRenderPass("ForwardTranslucencyPass"));
		if (TransPass)
		{
			TransPass->SetRenderArea(SwapchainExtent);
		}

		// Collect light data from scene
		// TODO: make better gathering, maybe throuhg registration
		std::vector<GPULightData> Lights;
		for (Entity* E : SceneToRender->GetAllEntities())
		{
			auto* TC = E->GetComponent<TransformComponent>();
			if (!TC)
			{
				continue;
			}

			auto LightComponents = E->GetComponentsOfBaseType<LightComponentBase>();
			for (auto* Base : LightComponents)
			{
				GPULightData LightData;
				LightData.Color_Intensity = glm::vec4(Base->GetColor(), Base->GetIntensity());

				glm::quat Rot = TC->GetWorldRotation();
				glm::vec3 Forward = Rot * glm::vec3(0.0f, 0.0f, -1.0f);

				switch (Base->GetType())
				{
				case LightType::Directional:
					LightData.Direction = glm::vec4(Forward, 0.0f);
					LightData.Params = glm::vec4(0.0f, 0.0f, 0.0f, float(LightType::Directional));
					break;

				case LightType::Point:
					LightData.Position = glm::vec4(TC->GetWorldPosition(), 1.0f);
					LightData.Params = glm::vec4(static_cast<PointLightComponent*>(Base)->GetRange(),
						0.0f, 0.0f, float(LightType::Point));
					break;

				case LightType::Spot:
					LightData.Position = glm::vec4(TC->GetWorldPosition(), 1.0f);
					LightData.Direction = glm::vec4(Forward, 0.0f);
					{
						auto* Spot = static_cast<SpotLightComponent*>(Base);
						LightData.Params = glm::vec4(Spot->GetRange(),
							glm::cos(Spot->GetInnerConeAngle()),
							glm::cos(Spot->GetOuterConeAngle()),
							float(LightType::Spot));
					}
					break;
				}

				Lights.push_back(LightData);
			}
		}


		LightBufferInstance->Update(CurrentFrameData.FrameIndex, Lights);


		// Record rendering commands into command buffer using rendergraph
		RendergraphInstance->Execute(Cmd, GraphicsQueue, CurrentFrameData, ProfilerInstance.get(), CurrentFrame);
	}

	// Command submission with sync
	// Submit buffer
	Cmd.end();

	vk::PipelineStageFlags WaitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	vk::SubmitInfo SubmitInfo;
	SubmitInfo.setCommandBufferCount(1)		// Single command buffer
		.setPCommandBuffers(&*Cmd)			// Command buffer to exec
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(&*ImageAvailableSemaphores[CurrentFrame])
		.setPWaitDstStageMask(WaitStages)
		.setSignalSemaphoreCount(1)
		.setPSignalSemaphores(&*RenderFinishedSemaphores[ImageIndex]);


	GraphicsQueue.submit(SubmitInfo, *InFlightFences[CurrentFrame]);   // Submit to GPU queue

	// Present result
	vk::PresentInfoKHR PresentInfo;
	PresentInfo.setImageIndices(ImageIndex)
			   .setWaitSemaphoreCount(1)
			   .setPWaitSemaphores(&*RenderFinishedSemaphores[ImageIndex])
			   .setSwapchainCount(1)
			   .setPSwapchains(&*Swapchain);

	vk::Result PresentResult = vk::Result::eErrorUnknown;
	try {
		PresentResult = PresentQueue.presentKHR(PresentInfo);
	}
	catch (const vk::OutOfDateKHRError&) {
		PresentResult = vk::Result::eErrorOutOfDateKHR;
	}
	catch (const vk::SystemError& e) {
		throw;   // re‑throw other unexpected errors
	}

	if (PresentResult == vk::Result::eErrorOutOfDateKHR ||
		PresentResult == vk::Result::eSuboptimalKHR) {
		RecreateSwapchain();
	}
	else if (PresentResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to present swapchain image");
	}

	CurrentFrame = (CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT; // Advance to next frame index
}

void Renderer::RecreateSwapchain()
{
	// Handle minimization of window
	// Make sure that extent is not zero before proceeding due to perfomance
	auto SurfaceCaps = PhysicalDevice.getSurfaceCapabilitiesKHR(*Surface);

	if (SurfaceCaps.currentExtent.width == 0 ||
		SurfaceCaps.currentExtent.height == 0)
	{
		return; // Minimized window, skip
	}
	
	Device.waitIdle(); // Wait for device to be idle before recreating swapchain

	// Clean up old swapchain resources
	SwapchainImageViews.clear();
	Swapchain.clear(); // vk::raii::SwapchainKHR will automatically clean up old swapchain and related resources when reassigned

	// Create new swapchain with updated dimensions
	CreateSwapchain();

	// Recreate framebuffers and render passes that depend on swapchain images 
	
	// Reset descriptor sets that depend on swapchain images and rendregraph resources
	if (GBufferDescSet)
	{
		GBufferDescSet->ResetDescriptorSet();
	}
	if (PostProcessDesc)
	{
		PostProcessDesc->ResetDescriptorSet();
	}
	
	RendergraphInstance->Reset(); // Clear existing rendergraph resources and passes
	SetupRenderPasses();
	RendergraphInstance->Compile();

	if (GBufferDescSet)
	{
		GBufferDescSet->Initialize(*RendergraphInstance);
	}
	if (PostProcessDesc)
	{
		PostProcessDesc->Initialize(*RendergraphInstance);
	}
	CreateSyncObjects(); // Recreate synchronization objects if they depend on swapchain (e.g. semaphores for each swapchain image)
}

void Renderer::SetActiveCamera(CameraComponent* Camera)
{
	CullingSystemInstance->SetActiveCamera(Camera);

	// Update camera component aspect ratio to match swapchain extent
	// TODO: camera should be the one who changes aspect ration upon resize
	if (Camera)
	{
		float AspectRatio = static_cast<float>(SwapchainExtent.width) / static_cast<float>(SwapchainExtent.height);
		Camera->SetPerspective(Camera->GetFieldOfView(), AspectRatio, Camera->GetNearPlane(), Camera->GetFarPlane());
	}
}

void Renderer::PickPhysicalDevice()
{
	vk::raii::PhysicalDevices Devices(Instance);
	if (Devices.empty())
	{
		throw std::runtime_error("No Vulkan-compatible GPU found");
	}

	// Score devices and pick the best one
	int BestScore = -1;
	vk::raii::PhysicalDevice BestDevice = nullptr;

	for (auto& Dev : Devices)
	{
		// Must support present and graphics family

		auto QueueFamilies = Dev.getQueueFamilyProperties();
		bool bHasGraphics = false;
		bool bHasPresent = false;

		for (uint32_t i = 0; i < QueueFamilies.size(); i++)
		{
			if(QueueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
			{
				bHasGraphics = true;
			}
			
			if(Dev.getSurfaceSupportKHR(i, *Surface))
			{
				bHasPresent = true;
			}

			if(bHasGraphics && bHasPresent)
			{
				break;
			}
		}

		if (!bHasGraphics || !bHasPresent)
		{
			continue; // Skip devices that don't support required queue families
		}

		int Score = ScorePhysicalDevice(Dev, Surface);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestDevice = Dev;
		}
	}

	if(BestDevice == nullptr)
	{
		throw std::runtime_error("No suitable GPU found");
	}

	PhysicalDevice = BestDevice;
}

void Renderer::CreateLogicalDevice()
{
	// Find queue family indices (graphics and present)

	auto QueueFamilies = PhysicalDevice.getQueueFamilyProperties();
		
	auto FindGraphicsAndPresent = [&]() -> std::optional<uint32_t>
	{
		for (uint32_t i = 0; i < QueueFamilies.size(); i++)
		{
			const vk::QueueFlags RequiredFlags =
				vk::QueueFlagBits::eGraphics |
				vk::QueueFlagBits::eCompute;

			if((QueueFamilies[i].queueFlags & RequiredFlags) == RequiredFlags && PhysicalDevice.getSurfaceSupportKHR(i, *Surface))
			{
				return i; // Found a queue family that supports both graphics, present and compute
			}
		}
		return {};
	};

	auto FindTransfer = [&]() -> std::optional<uint32_t>
	{
		for (uint32_t i = 0; i < QueueFamilies.size(); i++)
		{
			if(QueueFamilies[i].queueFlags & vk::QueueFlagBits::eTransfer && !(QueueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics))
			{
				return i; // Found a dedicated transfer queue family
			}
		}
		// fallback: if no dedicated transfer queue, use graphics queue
		for (uint32_t i = 0; i < QueueFamilies.size(); i++)
		{
			if(QueueFamilies[i].queueFlags & vk::QueueFlagBits::eTransfer)
			{
				return i; // Found a transfer queue family (may also support graphics)
			}
		}
		return {};
	};

	auto GraphicsAndPresentFamily = FindGraphicsAndPresent();
	auto TransferFamily = FindTransfer();

	if (!GraphicsAndPresentFamily.has_value())
	{
		throw std::runtime_error("Failed to find a queue family that supports both graphics, present and compute");
	}

	std::vector<const char*> DeviceExtensions = {
	   VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	   VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
	};

	// Create logical device with required queues
	std::vector<vk::DeviceQueueCreateInfo> QueueCreateInfos;
	float QueuePriority = 1.0f;

	// Graphics and present queue (one queue for both)
	vk::DeviceQueueCreateInfo GraphicsQueueCreateInfo({}, GraphicsAndPresentFamily.value(), 1, &QueuePriority);
	QueueCreateInfos.push_back(GraphicsQueueCreateInfo);

	// Dedicated transfer queue
	if (TransferFamily.has_value() && TransferFamily.value() != GraphicsAndPresentFamily.value())
	{
		vk::DeviceQueueCreateInfo TransferQueueCreateInfo({}, TransferFamily.value(), 1, &QueuePriority);
		QueueCreateInfos.push_back(TransferQueueCreateInfo);
	}

	vk::PhysicalDeviceFeatures EnabledFeatures{};
	EnabledFeatures.samplerAnisotropy = VK_TRUE;

	vk::DeviceCreateInfo DeviceCreateInfo({}, QueueCreateInfos, {}, DeviceExtensions, &EnabledFeatures);

	// Enable dynamic rendering feature
	vk::PhysicalDeviceDynamicRenderingFeatures DynamicRenderingFeature;
	DynamicRenderingFeature.setDynamicRendering(VK_TRUE);

	// Enable descriptor indexing features 
	vk::PhysicalDeviceDescriptorIndexingFeatures DescriptorIndexingFeature;
	DescriptorIndexingFeature.setShaderSampledImageArrayNonUniformIndexing(VK_TRUE)		// nonuniformEXT() in shader (for indexing using push constants)
							 .setDescriptorBindingSampledImageUpdateAfterBind(VK_TRUE)	// update heap after bind
							 .setDescriptorBindingPartiallyBound(VK_TRUE)				// partially filled array
							 .setRuntimeDescriptorArray(VK_TRUE);						// variable array size

	// Enable shaderDrawParameters (for SV_VertexID in fullscreen triangle)
	vk::PhysicalDeviceVulkan11Features Vulkan11Features;
	Vulkan11Features.setShaderDrawParameters(VK_TRUE)
					.setPNext(nullptr);						// end of chain

	// Chain: DescriptorIndexing -> Vulkan11Features
	DescriptorIndexingFeature.setPNext(&Vulkan11Features);
	// Chain: DynamicRendering -> DescriptorIndexing
	DynamicRenderingFeature.setPNext(&DescriptorIndexingFeature);
	// Chain: DeviceCreateInfo -> DynamicRendering
	DeviceCreateInfo.setPNext(&DynamicRenderingFeature);

	Device = vk::raii::Device(PhysicalDevice, DeviceCreateInfo);

	GraphicsQueueFamilyIndex = GraphicsAndPresentFamily.value();

	// Get queue handles
	GraphicsQueue = Device.getQueue(GraphicsAndPresentFamily.value(), 0);
	PresentQueue = Device.getQueue(GraphicsAndPresentFamily.value(), 0); // Same queue for present
	if (TransferFamily.has_value() && TransferFamily.value() != GraphicsAndPresentFamily.value())
	{
		TransferQueue = Device.getQueue(*TransferFamily, 0);
		TransferQueueFamilyIndex = TransferFamily.value();
	}
	else
	{
		TransferQueue = GraphicsQueue; // Fallback: Use graphics queue for transfer if no dedicated transfer queue
		TransferQueueFamilyIndex = GraphicsAndPresentFamily.value();
	}
}

void Renderer::CreateSwapchain()
{
	auto SurfaceCaps = PhysicalDevice.getSurfaceCapabilitiesKHR(*Surface);
	auto SurfaceFormats = PhysicalDevice.getSurfaceFormatsKHR(*Surface);
	auto PresentModes = PhysicalDevice.getSurfacePresentModesKHR(*Surface);

	// Choose format that works even with overlay injections
	//
	// Overlays (RTSS, Steam, Discord, etc.) often hook into swapchain creation
	// and silently add VK_IMAGE_USAGE_STORAGE_BIT and VK_IMAGE_USAGE_TRANSFER_SRC_BIT
	// to the image usage. This causes a validation error (or even a crash) if the
	// chosen format does not support those extra flags.
	//
	// To avoid this, we ask the GPU what formats support our required usage PLUS
	// the extra usage that overlays typically add. If we find one, we use it.
	// Otherwise we fall back to a format that supports only our required usage.
	//
	// The visual output will always be correct sRGB because we pair the format
	// with VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, which tells the display hardware
	// to apply the sRGB gamma curve during presentation.

	vk::Format        SurfaceFormat = vk::Format::eUndefined;
	vk::ColorSpaceKHR SurfaceColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;

	vk::ImageUsageFlags RequiredUsage = vk::ImageUsageFlagBits::eColorAttachment
		| vk::ImageUsageFlagBits::eTransferDst;

	vk::ImageUsageFlags ExtraUsage = vk::ImageUsageFlagBits::eStorage
		| vk::ImageUsageFlagBits::eTransferSrc;

	// First pass: required + extra usage
	for (auto& Fmt : SurfaceFormats) 
	{
		if (IsFormatUsageSupported(Fmt.format, RequiredUsage | ExtraUsage)) 
		{
			SurfaceFormat = Fmt.format;
			SurfaceColorSpace = Fmt.colorSpace;
			break;
		}
	}

	// Second pass: fallback – required usage only
	if (SurfaceFormat == vk::Format::eUndefined) 
	{
		for (auto& Fmt : SurfaceFormats) 
		{
			if (IsFormatUsageSupported(Fmt.format, RequiredUsage)) 
			{
				SurfaceFormat = Fmt.format;
				SurfaceColorSpace = Fmt.colorSpace;
				break;
			}
		}
	}

	if (SurfaceFormat == vk::Format::eUndefined)
	{
		throw std::runtime_error("No suitable swapchain format found");
	}

	// Choose present mode (prefer mailbox, then FIFO)
	vk::PresentModeKHR PresentMode = vk::PresentModeKHR::eFifo; // Fallback default
	for (auto& AvailablePresentMode : PresentModes)
	{
		if(AvailablePresentMode == vk::PresentModeKHR::eMailbox)
		{
			PresentMode = AvailablePresentMode;
			break;
		}
	}

	// Determine swap extent (use surface capabilities)
	SwapchainExtent = SurfaceCaps.currentExtent;
	if (SwapchainExtent.width == UINT32_MAX)
	{
		// Surface size is not defined, fallback
		SwapchainExtent.width = std::clamp(1920u, SurfaceCaps.minImageExtent.width, SurfaceCaps.maxImageExtent.width);
		SwapchainExtent.height = std::clamp(1080u, SurfaceCaps.minImageExtent.height, SurfaceCaps.maxImageExtent.height);
	}

	// Image count: tripple buffering (min + 1, at least 3)
	uint32_t ImageCount = std::max(SurfaceCaps.minImageCount + 1, 3u);

	if (SurfaceCaps.maxImageCount > 0)
	{
		ImageCount = std::min(ImageCount, SurfaceCaps.maxImageCount); // Clamp to max if there is a limit
	}

	SwapchainMinImageCount = ImageCount;

	vk::SwapchainCreateInfoKHR SwapchainCreateInfo;

	SwapchainCreateInfo.setSurface(*Surface)
		.setMinImageCount(ImageCount)
		.setImageFormat(SurfaceFormat)
		.setImageColorSpace(SurfaceColorSpace)
		.setImageExtent(SwapchainExtent)
		.setImageArrayLayers(1)											
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)	// Usage
		.setImageSharingMode(vk::SharingMode::eExclusive)												// Pre-transform
		.setPreTransform(SurfaceCaps.currentTransform)
		.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
		.setPresentMode(PresentMode)																	// Sharing mode
		.setClipped(VK_TRUE);																			// Clipped

	Swapchain = vk::raii::SwapchainKHR(Device.createSwapchainKHR(SwapchainCreateInfo, nullptr));
	SwapchainImages = Swapchain.getImages();
	SwapchainImageFormat = SurfaceFormat;

	// Create image views for swapchain images
	SwapchainImageViews.clear();
	SwapchainImageViews.reserve(SwapchainImages.size());

	for (auto& Image : SwapchainImages)
	{
		vk::ImageViewCreateInfo ViewCreateInfo
		(
			{},						// Flags
			Image,					// Image
			vk::ImageViewType::e2D,
			SwapchainImageFormat,
			vk::ComponentMapping(),	// Component mapping (default)
			{						// Subresource range
				vk::ImageAspectFlagBits::eColor,
				0, 1,				// Mip levels
				0, 1				// Array layers
			}
		);
		SwapchainImageViews.emplace_back(Device, ViewCreateInfo); // Create and store image view
	}
}

void Renderer::CreateSyncObjects()
{
	ImageAvailableSemaphores.clear();
	RenderFinishedSemaphores.clear();
	InFlightFences.clear();
	
	// Per‑frame semaphores for acquisition
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		ImageAvailableSemaphores.emplace_back(Device, vk::SemaphoreCreateInfo());
	}

	// Per‑image semaphores for rendering finished (size = swapchain image count)
	for (size_t i = 0; i < SwapchainImages.size(); i++)
	{
		RenderFinishedSemaphores.emplace_back(Device, vk::SemaphoreCreateInfo());
	}

	// Fences stay per‑frame
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		InFlightFences.emplace_back(Device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));	// Start signaled so we can wait on it immediately
	}
}

void Renderer::SetupRenderPasses()
{
	RendergraphInstance->ImportExternalImage(
		"Swapchain",
		SwapchainImageFormat,
		SwapchainExtent,
		vk::ImageUsageFlagBits::eColorAttachment,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::ePresentSrcKHR);

	// G‑buffer color attachments (geometry pass outputs)
	RendergraphInstance->AddResource(
		"GBuffer_Albedo",
		vk::Format::eR8G8B8A8Unorm,					// RGBA8 – albedo.rgb, alpha (AO later)
		SwapchainExtent,
		vk::ImageUsageFlagBits::eColorAttachment |
		vk::ImageUsageFlagBits::eSampled,           // sampled by lighting pass
		vk::ImageAspectFlagBits::eColor,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eShaderReadOnlyOptimal     // lighting pass reads it
	);

	RendergraphInstance->AddResource(
		"GBuffer_Normal",
		vk::Format::eR16G16B16A16Sfloat,            // RGBA16F – world‑space normal
		SwapchainExtent,
		vk::ImageUsageFlagBits::eColorAttachment |
		vk::ImageUsageFlagBits::eSampled,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eShaderReadOnlyOptimal
	);

	RendergraphInstance->AddResource(
		"GBuffer_MetalRough",
		vk::Format::eR8G8B8A8Unorm,                 // RGBA8 – metallic, roughness
		SwapchainExtent,
		vk::ImageUsageFlagBits::eColorAttachment |
		vk::ImageUsageFlagBits::eSampled,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eShaderReadOnlyOptimal
	);

	RendergraphInstance->AddResource(
		"GBuffer_Emissive",
		vk::Format::eR16G16B16A16Sfloat,            // RGBA16F – emissive (HDR)
		SwapchainExtent,
		vk::ImageUsageFlagBits::eColorAttachment |
		vk::ImageUsageFlagBits::eSampled,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eShaderReadOnlyOptimal
	);

	// Lighting pass output + swapchain target
	RendergraphInstance->AddResource(
		"Main_Color",
		vk::Format::eR16G16B16A16Sfloat,
		SwapchainExtent,
		vk::ImageUsageFlagBits::eColorAttachment |	// lighting pass writes here
		vk::ImageUsageFlagBits::eTransferSrc |		// keep for potential blit / other use
		vk::ImageUsageFlagBits::eSampled,           // required for post‑processing sampling
		vk::ImageAspectFlagBits::eColor,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eShaderReadOnlyOptimal
	);

	// Depth / stencil (shared by geometry and lighting passes)
	RendergraphInstance->AddResource(
		"Main_Depth",
		vk::Format::eD32Sfloat,
		SwapchainExtent,
		vk::ImageUsageFlagBits::eDepthStencilAttachment |
		vk::ImageUsageFlagBits::eSampled,
		vk::ImageAspectFlagBits::eDepth,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eDepthStencilAttachmentOptimal
	);

	// GBuffer descriptor set
	if (!GBufferDescSet)
	{
		GBufferDescSet = std::make_unique<GBufferDescriptorSet>(
			Device,
			"GBuffer_Albedo",
			"GBuffer_Normal",
			"GBuffer_MetalRough",
			"GBuffer_Emissive",
			"Main_Depth");
	}

	Shader* GeometryShader = LoadShader("deferred_geometry");
	
	// Geometry pass — writes all 4 G‑buffer attachments + depth
	auto GeomtryPass = RendergraphInstance->AddRenderPass<GeometryRenderPass>(
		"GeometryPass",
		"GBuffer_Albedo",
		"GBuffer_Normal",
		"GBuffer_MetalRough",
		"GBuffer_Emissive",
		"Main_Depth",
		GeometryShader,
		PipelineCacheInstance.get(),
		CameraUBO.get(),
		DescriptorHeapInstance.get(),
		GPUSceneInstance.get());

	Shader* LightingShader = LoadShader("deferred_lighting");

	auto LightPass = RendergraphInstance->AddRenderPass<LightingPass>(
		"LightPass",
		"Main_Color",
		"GBuffer_Albedo", "GBuffer_Normal", "GBuffer_MetalRough", "GBuffer_Emissive",
		"Main_Depth",
		CameraUBO.get(),
		LightBufferInstance.get(),
		GBufferDescSet.get(),
		LightingShader,
		PipelineCacheInstance.get());

	Shader* TranslucencyShader = LoadShader("forward_translucency");

	auto TranslucencyPass = RendergraphInstance->AddRenderPass<ForwardTranslucencyPass>(
		"ForwardTranslucencyPass",
		"Main_Color",
		"Main_Depth",
		TranslucencyShader,
		PipelineCacheInstance.get(),
		CameraUBO.get(),
		DescriptorHeapInstance.get(),
		GPUSceneInstance.get(),
		LightBufferInstance.get());

	Shader* PostProcessShader = LoadShader("post_process");

	if (!PostProcessDesc)
	{
		PostProcessDesc = std::make_unique<SingleTextureDescriptorSet>(Device, "Main_Color");
	}

	RendergraphInstance->AddRenderPass<PostProcessPass>(
		"PostProcess",
		"Main_Color",
		"Swapchain",
		PostProcessShader,
		PipelineCacheInstance.get(),
		PostProcessDesc.get(),
		CurrentPostProcessSettings);

	RendergraphInstance->AddRenderPass<ImGuiRenderPass>(
		"ImGuiPass",
		"Swapchain");
}

void Renderer::PreloadSceneResources(Scene& Scene)
{
	std::vector<std::string> TextureIDs;
	std::vector<const TextureData*> TextureDataPtrs;
	std::unordered_set<std::string> SeenTextures;
	std::vector<std::string> MeshIDs;
	std::vector<const MeshData*> MeshDataPtrs;
	std::unordered_set<std::string> SeenMeshes;

	for (Entity* E : Scene.GetAllEntities())
	{
		CollectEntityResources(*E,
			SeenTextures, TextureIDs, TextureDataPtrs,
			SeenMeshes, MeshIDs, MeshDataPtrs);
	}

	if (!MeshIDs.empty())
	{
		std::cout << "[Renderer] Preloading " << MeshIDs.size() << " meshes...\n";
		RenderCacheInstance->GetOrUploadMeshBatch(MeshIDs, MeshDataPtrs);
	}
	if (!TextureIDs.empty())
	{
		std::cout << "[Renderer] Preloading " << TextureIDs.size() << " textures...\n";
		RenderCacheInstance->GetOrUploadTextureBatch(TextureIDs, TextureDataPtrs);
	}
}

void Renderer::PreloadEntityResources(Entity& Entity)
{
	std::vector<std::string> TextureIDs;
	std::vector<const TextureData*> TextureDataPtrs;
	std::unordered_set<std::string> SeenTextures;
	std::vector<std::string> MeshIDs;
	std::vector<const MeshData*> MeshDataPtrs;
	std::unordered_set<std::string> SeenMeshes;

	CollectEntityResources(Entity,
		SeenTextures, TextureIDs, TextureDataPtrs,
		SeenMeshes, MeshIDs, MeshDataPtrs);

	if (!MeshIDs.empty())
	{
		RenderCacheInstance->GetOrUploadMeshBatch(MeshIDs, MeshDataPtrs);
	}
	if (!TextureIDs.empty())
	{
		RenderCacheInstance->GetOrUploadTextureBatch(TextureIDs, TextureDataPtrs);
	}
}

Shader* Renderer::LoadShader(const std::string& Name)
{
	ResourceHandle<ShaderData> Data = ResourceManagerPtr->GetHandle<ShaderData>(Name);

	if (!Data)
	{
		Data = ResourceManagerPtr->Load<ShaderData>(Name);
	}

	if (!Data)
	{
		throw std::runtime_error("Failed to load shader: " + Name);
	}

	return RenderCacheInstance->GetOrCompileShader(Data->GetResourceID(), *Data);
}

void Renderer::CollectEntityResources(
	Entity& Entity, 
	std::unordered_set<std::string>& SeenTextures, 
	std::vector<std::string>& TextureIDs, 
	std::vector<const TextureData*>& TextureDataPtrs, 
	std::unordered_set<std::string>& SeenMeshes, 
	std::vector<std::string>& MeshIDs, 
	std::vector<const MeshData*>& MeshDataPtrs)
{
	auto* MC = Entity.GetComponent<MeshComponent>();
	if (!MC)
	{
		return;
	}

	// Mesh
	const MeshData* MD = MC->GetMeshData();
	if (MD && SeenMeshes.insert(MD->GetResourceID()).second)
	{
		if (!RenderCacheInstance->IsMeshCached(MD->GetResourceID()))
		{
			MeshIDs.push_back(MD->GetResourceID());
			MeshDataPtrs.push_back(MD);
		}
	}

	// Textures
	const Material* Mat = MC->GetMaterial();
	if (!Mat) 
	{
		return;
	}

	auto Collect = [&](const ResourceHandle<TextureData>& Handle)
		{
			const TextureData* TD = Handle.Get();
			if (!TD) 
			{
				return;
			}
			if (!SeenTextures.insert(TD->GetResourceID()).second) 
			{
				return;
			}
			if (!RenderCacheInstance->IsTextureCached(TD->GetResourceID()))
			{
				TextureIDs.push_back(TD->GetResourceID());
				TextureDataPtrs.push_back(TD);
			}
		};

	Collect(Mat->GetAlbedoTexture());
	Collect(Mat->GetNormalTexture());
	Collect(Mat->GetMetallicRoughnessTexture());
	Collect(Mat->GetOcclusionTexture());
	Collect(Mat->GetEmissiveTexture());
}

int Renderer::ScorePhysicalDevice(const vk::raii::PhysicalDevice& Dev, const vk::raii::SurfaceKHR& Surface)
{
	auto Props = Dev.getProperties();
	auto Features = Dev.getFeatures();

	// Hard requirement — a device without this feature can't run the engine's texture pipeline correctly (anisotropic filtering assumed available).
	if (!Features.samplerAnisotropy)
	{
		return -1; // disqualify this device entirely, same effect as missing graphics/present support
	}

	// Memory size (device local)
	auto MemProps = Dev.getMemoryProperties();
	uint32_t VRAM = 0;

	for (auto& Heap : MemProps.memoryHeaps)
	{
		if(Heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal)
		{
			VRAM += Heap.size;
		}
	}

	int Score = 0;
	if (Props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
	{
		Score += 1000; // Discrete GPUs are preferred
	}
	else if (Props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
	{
		Score += 500; // Integrated GPUs are less powerful but more power efficient
	}

	Score += static_cast<int>(Props.limits.maxImageDimension2D / 1000); // Higher max texture size is better
	Score += static_cast<int>(VRAM / (1024 * 1024 * 100)); // More VRAM is better, per 100MB

	return Score;
}

bool Renderer::IsFormatUsageSupported(vk::Format Format, vk::ImageUsageFlags Usage)
{
	try {
		vk::ImageFormatProperties ImageFormatProps = PhysicalDevice.getImageFormatProperties(
			Format,
			vk::ImageType::e2D,
			vk::ImageTiling::eOptimal,
			Usage);
		return true;
	}
	catch (...) {
		return false;
	}
}

bool Renderer::CanAcquireSwapchainImage() const
{
	auto Caps = PhysicalDevice.getSurfaceCapabilitiesKHR(*Surface);
	// If the surface is minimised, extent will be zero
	if (Caps.currentExtent.width == 0 || Caps.currentExtent.height == 0)
		return false;

	// Optional: check if swapchain is too old vs current transform, etc.
	// maybe also compare swapchainExtent with caps.currentExtent if needed.

	return true;
}

void Renderer::InitializeImGuiVulkanBackend()
{
	if (bImGuiVulkanBackendInitialized)
	{
		return;
	}

	if (ImGui::GetCurrentContext() == nullptr)
	{
		throw std::runtime_error("Cannot initialize ImGui Vulkan backend without an ImGui context");
	}

	const uint32_t ActualImageCount = static_cast<uint32_t>(SwapchainImages.size());

	if (SwapchainMinImageCount < 2 || ActualImageCount < 2)
	{
		throw std::runtime_error("ImGui Vulkan backend requires at least two swapchain images");
	}

	VkFormat ColorAttachmentFormat = static_cast<VkFormat>(SwapchainImageFormat);

	ImGui_ImplVulkan_InitInfo InitInfo{};

	InitInfo.ApiVersion = VK_API_VERSION_1_4;
	InitInfo.Instance = static_cast<VkInstance>(*Instance);
	InitInfo.PhysicalDevice = static_cast<VkPhysicalDevice>(*PhysicalDevice);
	InitInfo.Device = static_cast<VkDevice>(*Device);
	InitInfo.QueueFamily = GraphicsQueueFamilyIndex;
	InitInfo.Queue = static_cast<VkQueue>(*GraphicsQueue);

	// Let the pinned ImGui backend create and own its private pool.
	InitInfo.DescriptorPool = VK_NULL_HANDLE;
	InitInfo.DescriptorPoolSize = 128;

	InitInfo.MinImageCount = SwapchainMinImageCount;
	InitInfo.ImageCount = ActualImageCount;
	InitInfo.PipelineCache = VK_NULL_HANDLE;
	InitInfo.UseDynamicRendering = true;
	InitInfo.PipelineInfoMain.RenderPass = VK_NULL_HANDLE;
	InitInfo.PipelineInfoMain.Subpass = 0;
	InitInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	auto& RenderingInfo = InitInfo.PipelineInfoMain.PipelineRenderingCreateInfo;

	RenderingInfo = {};
	RenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	RenderingInfo.colorAttachmentCount = 1;
	RenderingInfo.pColorAttachmentFormats = &ColorAttachmentFormat;
	InitInfo.CheckVkResultFn = &Renderer::CheckImGuiVulkanResult;

	if (!ImGui_ImplVulkan_Init(&InitInfo))
	{
		throw std::runtime_error("Failed to initialize ImGui Vulkan backend");
	}

	bImGuiVulkanBackendInitialized = true;
}

void Renderer::ShutdownImGuiVulkanBackend()
{
	if (!bImGuiVulkanBackendInitialized)
	{
		return;
	}

	ImGui_ImplVulkan_Shutdown();
	bImGuiVulkanBackendInitialized = false;
}

void Renderer::CheckImGuiVulkanResult(VkResult Result)
{
	if (Result < VK_SUCCESS)
	{
		throw std::runtime_error("ImGui Vulkan backend error: " + std::to_string(static_cast<int>(Result)));
	}
}

EventReply Renderer::OnEvent(const EventBase& Event)
{
	EventDispatcher Dispatcher(Event);

	Dispatcher.Dispatch<WindowResizeEvent>([this](const WindowResizeEvent& E)
	{
		RecreateSwapchain();
	});

	Dispatcher.Dispatch<SceneBulkChangedEvent>([this](const SceneBulkChangedEvent& E)
	{
		if (E.GetScene())
		{
			PreloadSceneResources(*E.GetScene());
		}
	});

	Dispatcher.Dispatch<SceneEntityChangedEvent>([this](const SceneEntityChangedEvent& E)
	{
		if (E.GetEntity())
		{
			PreloadEntityResources(*E.GetEntity());
		}
	});

	Dispatcher.Dispatch<ResourceReprocessedEvent>([this](const ResourceReprocessedEvent& E)
	{
		// Meshes: next RenderFrame's GetOrUploadMesh call for this ID naturally re-uploads
		RenderCacheInstance->Evict(E.GetResourceID());

		// TODO: Handle shaders here too
	});

	Dispatcher.Dispatch<PostProcessSettingsChangedEvent>([this](const PostProcessSettingsChangedEvent& E)
	{
		CurrentPostProcessSettings = E.GetSettings();
	});

	return EventReply::Unhandled;
}
