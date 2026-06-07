module;

#include <fstream>
#include <stdexcept>
#include <vulkan/vulkan_raii.hpp>

module Shader;

std::unique_ptr<ShaderModule> ShaderModule::Create(const vk::raii::Device& Device, const std::vector<uint32_t>& Code)
{
	vk::ShaderModuleCreateInfo createInfo({}, Code);
	vk::raii::ShaderModule module(Device, createInfo);
	return std::unique_ptr<ShaderModule>(new ShaderModule(std::move(module)));
}

std::unique_ptr<Shader> Shader::CreateFromBytecode(
	const vk::raii::Device& Device, 
	const std::vector<uint32_t>& VertexBytecode, 
	const std::vector<uint32_t>& FragmentBytecode)
{
	auto Vert = ShaderModule::Create(Device, VertexBytecode);
	auto Frag = ShaderModule::Create(Device, FragmentBytecode);
	return std::unique_ptr<Shader>(new Shader(std::move(Vert), std::move(Frag)));
}

std::unique_ptr<Shader> Shader::CreateGraphicsShader(
	const vk::raii::Device& Device, 
	const std::string& VertexShaderPath, 
	const std::string& FragmentShaderPath)
{
	// Read vertex SPIR‑V
	std::ifstream VertFile(VertexShaderPath, std::ios::binary | std::ios::ate);
	if (!VertFile)
		throw std::runtime_error("Failed to open vertex shader: " + VertexShaderPath);
	auto vertSize = VertFile.tellg();
	std::vector<uint32_t> vertCode(vertSize / sizeof(uint32_t));
	VertFile.seekg(0);
	VertFile.read(reinterpret_cast<char*>(vertCode.data()), vertSize);
	VertFile.close();

	// Read fragment SPIR‑V
	std::ifstream FragFile(FragmentShaderPath, std::ios::binary | std::ios::ate);
	if (!FragFile)
		throw std::runtime_error("Failed to open fragment shader: " + FragmentShaderPath);
	auto fragSize = FragFile.tellg();
	std::vector<uint32_t> fragCode(fragSize / sizeof(uint32_t));
	FragFile.seekg(0);
	FragFile.read(reinterpret_cast<char*>(fragCode.data()), fragSize);
	FragFile.close();

	return CreateFromBytecode(Device, vertCode, fragCode);
}

std::vector<vk::PipelineShaderStageCreateInfo> Shader::GetShaderStageInfos() const
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