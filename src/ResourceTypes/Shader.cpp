module;

#include <optional>
#include <string>
#include <vulkan/vulkan_raii.hpp>

module Shader;

bool Shader::LoadResource()
{
	// TODO: isn't shader code cannot repeat across shader stage types
	
	std::string Extension;
	switch (ShaderCodeType)
	{
	case vk::ShaderStageFlagBits::eVertex:
		{
			Extension = ".vert";
			break;
		}
	case vk::ShaderStageFlagBits::eFragment:
		{
			Extension = ".frag";
			break;
		}
	case vk::ShaderStageFlagBits::eCompute:
		{
			Extension = ".comp";
			break;
		}
	}

	std::string FilePath = "assets/shaders/" + GetResourceID() + Extension;

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
