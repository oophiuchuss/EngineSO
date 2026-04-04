module;

#include <optional>
#include <string>
#include <vulkan/vulkan_raii.hpp>

export module Shader;

import ResourceBase;

export class Shader : public ResourceBase
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
	bool LoadResource() override;
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
};