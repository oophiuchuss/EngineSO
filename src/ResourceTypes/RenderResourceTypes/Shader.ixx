module;

#include <optional>
#include <string>
#include <vulkan/vulkan_raii.hpp>

export module Shader;

export class ShaderModule
{
public:
	static std::unique_ptr<ShaderModule> Create(
		const vk::raii::Device& Device,
		const std::vector<uint32_t>& Code);

	ShaderModule(const ShaderModule&) = delete;					// Disable copy constructor
	ShaderModule& operator=(const ShaderModule&) = delete;		// Disable copy assignment
	ShaderModule(ShaderModule&&) noexcept = default;			// Enable move constructor
	ShaderModule& operator=(ShaderModule&&) noexcept = default; // Enable move assignment
	~ShaderModule() = default;									// Default destructor

	const vk::ShaderModule GetShaderModule() const { return *ShaderModulePtr; }

private:
	explicit ShaderModule(vk::raii::ShaderModule&& InShaderModule) : ShaderModulePtr(std::move(InShaderModule)) {}

	vk::raii::ShaderModule ShaderModulePtr = nullptr;
};

export class Shader
{
public:
	// Factory: create a Shader instance from SPIR-V bytecode for vertex and fragment shaders
	static std::unique_ptr<Shader> CreateFromBytecode(
		const vk::raii::Device& Device,
		const std::vector<uint32_t>& VertexBytecode,
		const std::vector<uint32_t>& FragmentBytecode);
	
	Shader(const Shader&) = delete;					// Disable copy constructor
	Shader& operator=(const Shader&) = delete;		// Disable copy assignment
	Shader(Shader&&) noexcept = default;			// Enable move constructor
	Shader& operator=(Shader&&) noexcept = default; // Enable move assignment
	~Shader() = default;							// Default destructor

	// Build a Vulkan pipeline shader stage create infos for this shader (vertex and fragment)
	std::vector<vk::PipelineShaderStageCreateInfo> GetShaderStageInfos() const;

private:
	Shader(std::unique_ptr<ShaderModule> VertexShader,
		std::unique_ptr<ShaderModule> FragmentShader)
		: VertexShader(std::move(VertexShader)),
		FragmentShader(std::move(FragmentShader)) {}

	std::unique_ptr<ShaderModule> VertexShader;   // Vertex shader module
	std::unique_ptr<ShaderModule> FragmentShader; // Fragment shader module
};
