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