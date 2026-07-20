module;

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

export module CameraUniform;

// Data layout – matches the shader’s uniform block
export struct CameraUniformData {
    glm::mat4 ViewProj;
	glm::mat4 InverseViewProj;
    glm::vec4 CameraPos;   // xyz = position, w = unused
};

// Manages the Vulkan buffer, memory, descriptor set layout, pool, and set
export class CameraUniformBuffer
{
public:
	CameraUniformBuffer(const vk::raii::Device& Device,
		const vk::raii::PhysicalDevice& PhysicalDevice,
		uint32_t FramesInFlight);

	~CameraUniformBuffer();

	// Call once per frame to update the uniform buffer with the latest camera data
	void Update(uint32_t FrameIndex, const CameraUniformData& Data);

	// Get the descriptor set layout - needed for creating pipeline layouts
	const vk::raii::DescriptorSet& GetDescriptorSet(uint32_t FrameIndex) const;
	const vk::raii::DescriptorSetLayout& GetDescriptorSetLayout() const { return DescriptorSetLayout; }
	
	// Get the last updated camera data
	const CameraUniformData& GetLastData() const { return LastData; }

private:
	struct FrameResources
	{
		vk::raii::DeviceMemory Memory = nullptr;
		vk::raii::Buffer Buffer = nullptr;
		void* MappedMemory = nullptr;

		vk::raii::DescriptorSet DescriptorSet = nullptr;
	};

	void CreateBuffer();
	void CreateDescriptor();

	FrameResources& GetFrameResources(uint32_t FrameIndex);
	const FrameResources& GetFrameResources(uint32_t FrameIndex) const;

	const vk::raii::Device& Device;
	const vk::raii::PhysicalDevice& PhysicalDevice;

	uint32_t FramesInFlight;

	vk::raii::DescriptorSetLayout DescriptorSetLayout = nullptr;
	vk::raii::DescriptorPool DescriptorPool = nullptr;
	std::vector<FrameResources> Frames;

	// This is only a CPU-side copy. The GPU never receives a pointer to it.
	CameraUniformData LastData{}; // Cache the last data for later retrieval if needed
};
