module;

#include <optional>
#include <string>
#include <vulkan/vulkan_raii.hpp>

module Shader;

bool Shader::LoadResource(const std::string& FilePath)
{
	// TODO: isn't shader code cannot repeat across shader stage types

	std::vector<char> ShaderCode;
	if (!ReadShaderCode(FilePath, ShaderCode))
	{
		return false; // Failed to read shader code from file
	}

	CreateShaderModule(ShaderCode);

	return true;
}

void Shader::UnloadResource()
{
	ShaderModule.reset();
	ShaderCodeType = {};
}
