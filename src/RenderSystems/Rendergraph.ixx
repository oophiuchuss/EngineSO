module;

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <vulkan/vulkan_raii.hpp>

export module Rendergraph;

// Resource description and management structure
// Represents Image resurce used during rendering (texture)
export struct Resource
{
	// Default constructor
	Resource() = default;

	// Parameterized constructor
	Resource(const std::string& name, vk::Format format, vk::Extent2D extent,
		vk::ImageLayout initialLayout, vk::ImageLayout finalLayout)
		: Name(name)
		, Format(format)
		, Extent(extent)
		, InitialLayout(initialLayout)
		, FinalLayout(finalLayout)
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
	vk::ImageLayout InitialLayout;	// Expected layout when the frame begins
	vk::ImageLayout FinalLayout;	// Required layout when the frame ens

	// Actual GPU handles - populated during compilation
	vk::raii::Image Image = nullptr;			// GPU image object
	vk::raii::DeviceMemory Memory = nullptr;	// Backing memory allocation
	vk::raii::ImageView View = nullptr;			// Shader-accessible view of the image
};

// Render pass representation within the graph structure
// Each pass represents an operation with distinct inputs and outputs
export struct Pass
{
	std::string Name;					// Human readable name for debugging
	std::vector<std::string> Inputs;	// Resources this pass will read (dependencies)
	std::vector<std::string> Outputs;	// Resources this pass will write to (products)

	std::function<void(vk::raii::CommandBuffer&)> ExecuteFunc; // Rendering code
};


export class Rendergraph
{
public:
	explicit Rendergraph(vk::raii::Device& Device, vk::raii::PhysicalDevice& PhysicalDevice) : Device(Device), PhysicalDevice(PhysicalDevice) {};

	void Compile();

	void Execute(vk::raii::CommandBuffer& CommandBuffer, vk::Queue Queue);

	void AddResource(
		const std::string& Name,
		vk::Format Format,
		vk::Extent2D Extent,
		vk::ImageLayout InitLayout,
		vk::ImageLayout FinalLayout);

	void AddPass(
		const std::string& Name,
		const std::vector<std::string>& Inputs,
		const std::vector<std::string>& Outputs,
		std::function<void(vk::raii::CommandBuffer&)> ExecuteFunc);

	Resource* GetResource(const std::string& Name);

private:
	uint32_t FindMemoryType(const uint32_t& TypeFilter, const vk::MemoryPropertyFlags& Properties) const;

	std::unordered_map<std::string, Resource> Resources; // All referenced resource
	std::vector<Pass> Passes;							 // All rendering passes in definition order
	std::vector<size_t> ExecOrder;						 // Optimal execution order

	// Syncronization management
	std::vector<vk::raii::Semaphore> Semaphores; // Sync primitives
	std::vector<std::pair<size_t, size_t>> SemaphoreWaitPairs; // Signaling pass | Waiting pass

	vk::raii::Device& Device;
	vk::raii::PhysicalDevice& PhysicalDevice;

};