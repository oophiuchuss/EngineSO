module;

#include <fstream>
#include <stdexcept>
#include <vulkan/vulkan_raii.hpp>

module Shader;

std::unique_ptr<ShaderModule> ShaderModule::LoadFromFile(
	const vk::raii::Device& Device, 
	const std::string& FilePath)
{
	// Read binary files
	
	std::ifstream File(FilePath, std::ios::binary | std::ios::ate);
	if (!File)
	{
		throw std::runtime_error("Failed to open shader file: " + FilePath);
	}

	auto FileSize = File.tellg();

	if (FileSize % sizeof(uint32_t) != 0)
	{
		throw std::runtime_error("Shader file size is not a multiple of uint32_t: " + FilePath);
	}

	std::vector<uint32_t> ShaderCode(FileSize / sizeof(uint32_t));
	File.seekg(0);
	File.read(reinterpret_cast<char*>(ShaderCode.data()), FileSize);
	File.close();
	
	// Create Vulkan shader module
	vk::ShaderModuleCreateInfo CreateInfo({}, ShaderCode);
	vk::raii::ShaderModule Module(Device, CreateInfo);

	return std::unique_ptr<ShaderModule>(new ShaderModule(std::move(Module)));
}

std::unique_ptr<Shader> Shader::CreateGraphicsShader(
	const vk::raii::Device& Device, 
	const std::string& VertexShaderPath, 
	const std::string& FragmentShaderPath)
{
	auto Vert = ShaderModule::LoadFromFile(Device, VertexShaderPath);
	auto Frag = ShaderModule::LoadFromFile(Device, FragmentShaderPath);

	return std::unique_ptr<Shader>(new Shader(std::move(Vert), std::move(Frag)));
}

std::vector<vk::PipelineShaderStageCreateInfo> Shader::GetVertexShaderStageInfo() const
{
	std::vector<vk::PipelineShaderStageCreateInfo> ShaderStages(2);

	ShaderStages[0] = vk::PipelineShaderStageCreateInfo({},
		vk::ShaderStageFlagBits::eVertex,
		VertexShader->GetShaderModule(),
		"main"
	);
	
	ShaderStages[1] = vk::PipelineShaderStageCreateInfo({},
		vk::ShaderStageFlagBits::eFragment,
		FragmentShader->GetShaderModule(),
		"main"
	);

	return ShaderStages;
}


/*
bool Shader::LoadResource(const std::string& FilePath)
{
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
}*/
