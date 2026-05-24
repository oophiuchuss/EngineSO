module;

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

export module CameraUniform;

// Data layout – matches the shader’s uniform block
export struct CameraUniformData {
    glm::mat4 ViewProj;
    glm::vec4 CameraPos;   // xyz = position, w = unused
};

// Manages the Vulkan buffer, memory, descriptor set layout, pool, and set
export class CameraUniformBuffer
{
public:
	CameraUniformBuffer(const vk::raii::Device& Device, const vk::raii::PhysicalDevice& PhysicalDevice);
	~CameraUniformBuffer() = default;

	// Call once per frame to update the uniform buffer with the latest camera data
	void Update(const CameraUniformData& Data);

	// Bind the uniform buffer to the command buffer before drawing
	void Bind(vk::CommandBuffer& Cmd, vk::PipelineLayout PipelineLayout, vk::PipelineBindPoint BindPoint = vk::PipelineBindPoint::eGraphics);

	// Get the descriptor set layout - needed for creating pipeline layouts
	const vk::raii::DescriptorSetLayout& GetDescriptorSetLayout() const { return DescriptorSetLayout; }

private:
	void CreateBuffer();
	void CreateDescriptor();

	const vk::raii::Device& Device;
	const vk::raii::PhysicalDevice& PhysicalDevice;

	vk::raii::Buffer Buffer = nullptr;
	vk::raii::DeviceMemory Memory = nullptr;

	void* MappedMemory = nullptr; // Pointer to the mapped memory for easy updates

	vk::raii::DescriptorPool DescriptorPool = nullptr;
	vk::raii::DescriptorSetLayout DescriptorSetLayout = nullptr;
	vk::raii::DescriptorSet DescriptorSet = nullptr;
};
