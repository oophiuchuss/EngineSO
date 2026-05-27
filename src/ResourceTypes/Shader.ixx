module;

#include <optional>
#include <string>
#include <vulkan/vulkan_raii.hpp>

export module Shader;

//import ResourceBase;

export class ShaderModule
{
public:
	// Factory: load SPIR-V shader from file and create a Shader instance
	static std::unique_ptr<ShaderModule> LoadFromFile(const vk::raii::Device& Device, const std::string& FilePath);

	ShaderModule(const ShaderModule&) = delete;					// Disable copy constructor
	ShaderModule& operator=(const ShaderModule&) = delete;		// Disable copy assignment
	ShaderModule(ShaderModule&&) noexcept = default;			// Enable move constructor
	ShaderModule& operator=(ShaderModule&&) noexcept = default; // Enable move assignment
	~ShaderModule() = default;									// Default destructor

	const vk::ShaderModule GetShaderModule() const { return *ShaderModulePtr; }

private:
	// Private constructor to enforce factory usage
	ShaderModule(vk::raii::ShaderModule&& ShaderModule) : ShaderModulePtr(std::move(ShaderModule)) {}

	vk::raii::ShaderModule ShaderModulePtr = nullptr;
};

export class Shader
{
public:
	// Create a graphics shader from vertex and fragment shader file paths
	static std::unique_ptr<Shader> CreateGraphicsShader(
		const vk::raii::Device& Device, 
		const std::string& VertexShaderPath, 
		const std::string& FragmentShaderPath);
	
	Shader(const Shader&) = delete;					// Disable copy constructor
	Shader& operator=(const Shader&) = delete;		// Disable copy assignment
	Shader(Shader&&) noexcept = default;			// Enable move constructor
	Shader& operator=(Shader&&) noexcept = default; // Enable move assignment
	~Shader() = default;							// Default destructor

	// Build a Vulkan pipeline shader stage create info for the vertex shader
	std::vector<vk::PipelineShaderStageCreateInfo> GetVertexShaderStageInfo() const;

private:
	Shader(std::unique_ptr<ShaderModule> VertexShader,
		std::unique_ptr<ShaderModule> FragmentShader)
		: VertexShader(std::move(VertexShader)),
		FragmentShader(std::move(FragmentShader)) {}

	std::unique_ptr<ShaderModule> VertexShader;   // Vertex shader module
	std::unique_ptr<ShaderModule> FragmentShader; // Fragment shader module
};

// TODO: reintegrate with ResourceBase and implement actual shader loading, Vulkan shader module creation, and memory management logic
/*export class Shader : public ResourceBase
{
public:
	Shader(const std::string& id, vk::ShaderStageFlagBits shaderCodeType) : ResourceBase(id), ShaderCodeType(shaderCodeType) {}

	~Shader()
	{
		Unload(); // Ensure resources are released when the shader is destroyed
	}

	// Accessors for Vulkan shader module handle and shader stage type, returning default-constructed handle if not loaded
	vk::ShaderModule GetShaderModule() const { return ShaderModule ? **ShaderModule : vk::ShaderModule{}; }
	vk::ShaderStageFlagBits GetShaderCodeType() const { return ShaderCodeType; }

protected:
	bool LoadResource(const std::string& FilePath) override;
	void UnloadResource() override;

private:
	// TODO: Implement actual shader loading, Vulkan shader module creation, and memory management logic
	// Placeholder functions for shader loading and Vulkan resource creation
	bool ReadShaderCode(const std::string& FilePath, std::vector<char>& OutShaderCode) { return false; }
	void CreateShaderModule(const std::vector<char>& ShaderCode) {}
	vk::Device GetDevice() const { return vk::Device(); }

	// Shader module management - stores the Vulkan shader module handle and its associated shader stage type
	std::optional<vk::raii::ShaderModule> ShaderModule; // Vulkan shader module handle
	vk::ShaderStageFlagBits ShaderCodeType;				// Type of shader code (e.g., vertex, fragment, compute, etc.)
};*/