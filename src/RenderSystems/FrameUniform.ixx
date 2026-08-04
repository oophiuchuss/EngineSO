module;

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

export module FrameUniform;

import TextureSlots;

// Data layout – matches the shader’s uniform block
export struct CameraUniformData
{
    // Current jittered view-projection used for rasterization.
    glm::mat4 ViewProj = glm::mat4(1.0f);

    glm::mat4 InverseViewProj = glm::mat4(1.0f);

    // View-projection used by the previous successfully submitted frame.
    glm::mat4 PreviousViewProj = glm::mat4(1.0f);

	glm::vec4 CameraPos = glm::vec4(0.0f);	// xyz = position, w = unused

    glm::vec4 PreviousCameraPos = glm::vec4(0.0f);

    // xy = current jitter in normalized texture UV units
    // zw = previous submitted jitter in normalized texture UV units
    glm::vec4 TemporalJitterUV = glm::vec4(0.0f);
};

export struct alignas(16) EnvironmentUniformData
{
	glm::mat4 WorldToEnvironment = glm::mat4(1.0f);

	uint32_t CubemapIndex = CubemapSlots::DefaultBlack;
	float Intensity = 1.0f;
	glm::vec2 Padding = glm::vec2(0.0f);
};

export struct FrameUniformData
{
	CameraUniformData Camera;
	EnvironmentUniformData Environment;
};

// Manages the Vulkan buffer, memory, descriptor set layout, pool, and set
export class FrameUniformBuffer
{
public:
    FrameUniformBuffer(
        const vk::raii::Device& InDevice,
        const vk::raii::PhysicalDevice& InPhysicalDevice,
        uint32_t InFramesInFlight);

    ~FrameUniformBuffer();

    // Call once per frame to update the uniform buffer with the latest camera data
    void Update(uint32_t FrameIndex, const FrameUniformData& Data);

    // Get the descriptor set layout - needed for creating pipeline layouts
    const vk::raii::DescriptorSet& GetDescriptorSet(uint32_t FrameIndex) const;
    const vk::raii::DescriptorSetLayout& GetDescriptorSetLayout() const { return DescriptorSetLayout; }

    // Get the last updated camera data
    const FrameUniformData& GetLastData() const { return LastData; }

private:
    struct UniformBufferResource
    {
        vk::raii::DeviceMemory Memory = nullptr;
        vk::raii::Buffer Buffer = nullptr;
        void* MappedMemory = nullptr;
    };

    struct FrameResources
    {
        UniformBufferResource Camera;
        UniformBufferResource Environment;

        vk::raii::DescriptorSet DescriptorSet = nullptr;
    };

    void CreateBuffers();
    void CreateDescriptors();

    void CreateUniformBuffer(vk::DeviceSize Size, UniformBufferResource& Output);

    void DestroyMappedBuffer(UniformBufferResource& Resource);

    FrameResources& GetFrameResources(uint32_t FrameIndex);

    const FrameResources& GetFrameResources(uint32_t FrameIndex) const;

    const vk::raii::Device& Device;
    const vk::raii::PhysicalDevice& PhysicalDevice;

    uint32_t FramesInFlight;

    vk::raii::DescriptorSetLayout DescriptorSetLayout = nullptr;
    vk::raii::DescriptorPool DescriptorPool = nullptr;

    std::vector<FrameResources> Frames;

    // This is only a CPU-side copy. The GPU never receives a pointer to it.
    FrameUniformData LastData{}; // Cache the last data for later retrieval if needed
};