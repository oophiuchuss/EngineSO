module;

#include <string>
#include <vector>

export module ShaderData;

import ResourceBase;
import Paths;

export enum class ShaderProgramType
{
    Graphics,
    Compute
};

export struct ShaderData : public ResourceBase
{
    explicit ShaderData(const std::string& ID) : ResourceBase(ID) {}

	// SPIR-V bytecode loaded from <ID>.vert.spv and <ID>.frag.spv or <ID>.comp.spv depending on the shader type
    std::vector<uint32_t> VertexBytecode;
    std::vector<uint32_t> FragmentBytecode;
	std::vector<uint32_t> ComputeBytecode;

	ShaderProgramType GetProgramType() const { return ProgramType; }

    static std::string GetRootPath() { return Paths::GetShadersRoot(); }
    static std::string_view AssetFolder() { return ""; }    // flat — no subfolder
    static std::string_view FileExtension() { return ""; }  // TODO: extensions are added during load, maybe should be fixed?

protected:
    bool LoadResource(const std::string& BasePath) override;
    void UnloadResource() override;

	ShaderProgramType ProgramType = ShaderProgramType::Graphics;

private:
    static bool ReadSpirV(const std::string& FilePath, std::vector<uint32_t>& OutBytecode);
};