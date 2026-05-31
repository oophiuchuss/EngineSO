module;

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <vulkan/vulkan_raii.hpp>

export module Rendergraph;

import RenderPassBase;
import FrameData;

// Resource description and management structure
// Represents Image resurce used during rendering (texture)
export struct Resource
{
	// Default constructor
	Resource() = default;

	// Parameterized constructor
	Resource(const std::string& InName, vk::Format InFormat, vk::Extent2D InExtent, vk::ImageUsageFlags InUsage, vk::ImageAspectFlags InAspect,
		vk::ImageLayout InInitialLayout, vk::ImageLayout InFinalLayout)
		: Name(InName)
		, Format(InFormat)
		, Extent(InExtent)
		, Usage(InUsage)
		, Aspect(InAspect)
		, InitialLayout(InInitialLayout)
		, FinalLayout(InFinalLayout)
	{
	}

	// Move operations (Vulkan objects support move)
	Resource(Resource&& other) noexcept = default;
	Resource& operator=(Resource&& other) noexcept = default;

	// Copy operations are deleted (Vulkan objects can't be copied)
	Resource(const Resource&) = delete;
	Resource& operator=(const Resource&) = delete;

	std::string Name;				// Human readable name for debugging
	vk::Format Format;				// Pixel format
	vk::Extent2D Extent;			// Dimenstions in pixels
	vk::ImageUsageFlags Usage;		// How resource will be used (color attachment, texture, etc.)
	vk::ImageAspectFlags Aspect;	// Which aspects (color, depth, stencil) 
	vk::ImageLayout InitialLayout;	// Expected layout when the frame begins
	vk::ImageLayout FinalLayout;	// Required layout when the frame ens

	// Actual GPU handles - populated during compilation
	vk::raii::DeviceMemory Memory = nullptr;	// Backing memory allocation
	vk::raii::Image Image = nullptr;			// GPU image object
	vk::raii::ImageView View = nullptr;			// Shader-accessible view of the image
};

export class Rendergraph
{
public:
	explicit Rendergraph(vk::raii::Device& Device, vk::raii::PhysicalDevice& PhysicalDevice) : Device(Device), PhysicalDevice(PhysicalDevice) {};

	void Compile();

	void Reset();

	void Execute(vk::raii::CommandBuffer& CommandBuffer, vk::Queue Queue, FrameData& CurrentFrameData);

	void TransitionResourceImageLayout(
		vk::raii::CommandBuffer& CommandBuffer,
		Resource* ResourcePtr,
		vk::ImageLayout OldLayout, 
		vk::ImageLayout NewLayout);

	void AddResource(
		const std::string& Name,
		vk::Format Format,
		vk::Extent2D Extent,
		vk::ImageUsageFlags Usage,
		vk::ImageAspectFlags Aspect,
		vk::ImageLayout InitLayout,
		vk::ImageLayout FinalLayout);

	template<typename T, typename... Args>
	T* AddRenderPass(const std::string& Name, Args&&... args)
	{
		static_assert(std::is_base_of<RenderPassBase, T>::value, "T must derive from RenderPassBase");

		auto it = Passes.find(Name);
		if (it != Passes.end())
		{
			return dynamic_cast<T*>(it->second.get());
		}

		auto NewPass = std::make_unique<T>(Name, std::forward<Args>(args)...);
		T* NewPassPtr = NewPass.get();
		Passes[Name] = std::move(NewPass);
		bIsPassesDirty = true;

		return NewPassPtr;
	}

	void RemoveRenderPass(const std::string& Name);

	inline RenderPassBase* GetRenderPass(const std::string& Name)
	{
		auto it = Passes.find(Name);
		if (it != Passes.end())
		{
			return it->second.get();
		}

		return nullptr;
	}

	inline Resource* GetResource(const std::string& Name)
	{
		auto it = Resources.find(Name);
		if (it != Resources.end())
		{
			return &(it->second);
		}

		return nullptr;
	}

private:
	void TransitionImageLayout(
		vk::raii::CommandBuffer& CommandBuffer,
		vk::Image Image,
		vk::ImageAspectFlags Aspect,
		vk::ImageLayout OldLayout,
		vk::ImageLayout NewLayout);

	void SortPasses();

	void TopologicalSort(
		const std::string& Name, 
		const std::unordered_map<std::string, std::vector<std::string>>& Dependencies,
		std::unordered_set<std::string>& Visited,
		std::unordered_set<std::string>& Visiting);

	std::unordered_map<std::string, Resource> Resources;						// All referenced resource
	std::unordered_map<std::string, std::unique_ptr<RenderPassBase>> Passes;	// All rendering passes 
	std::vector<RenderPassBase*> SortedPasses;									// Optimal execution order
	bool bIsPassesDirty = true;

	std::unordered_map<std::string, vk::ImageLayout> CurrentLayouts; // Current layouts of resources

	vk::raii::Device& Device;
	vk::raii::PhysicalDevice& PhysicalDevice;
};